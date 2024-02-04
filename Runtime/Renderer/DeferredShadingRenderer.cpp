#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::SetCurrentScene(UHScene* InScene)
{
	CurrentScene = InScene;
}

UHScene* UHDeferredShadingRenderer::GetCurrentScene() const
{
	return CurrentScene;
}

void UHDeferredShadingRenderer::SetSwapChainReset(bool bInFlag)
{
	bIsSwapChainResetGT = bInFlag;
}

bool UHDeferredShadingRenderer::IsNeedReset()
{
	// read Shared value to GT
	bool bIsNeedResetGT = bIsResetNeededShared;
	return bIsNeedResetGT;
}

// function for resize buffers, called when rendering resolution changes
void UHDeferredShadingRenderer::Resize()
{
	GraphicInterface->WaitGPU();

	ReleaseRenderPassObjects();
	RelaseRenderingBuffers();
	CreateRenderingBuffers();
	CreateRenderPasses();
	CreateRenderFrameBuffers();

	// need to rewrite descriptors after resize
	UpdateDescriptors();

	bIsTemporalReset = true;
}

// function for reset this interface, assume release is called
void UHDeferredShadingRenderer::Reset()
{
	GraphicInterface->WaitGPU();

	// reset simply reset all stuffs when bad things happened
	if (IsNeedReset())
	{
		Initialize(CurrentScene);
	}
}

// update function of renderer, uploading or culling happens here
void UHDeferredShadingRenderer::Update()
{
	if (!CurrentScene)
	{
		// don't update without a scene
		UHE_LOG(L"Scene is not set!\n");
		return;
	}

	// if updating has a huge spike in the future, consider assign the tasks to different worker thread
	UploadDataBuffers();
	FrustumCulling();
	CollectVisibleRenderer();
	SortRenderer();
}

void UHDeferredShadingRenderer::NotifyRenderThread()
{
	if (!CurrentScene)
	{
		// don't render without a scene
		UHE_LOG(L"Scene is not set!\n");
		return;
	}

	// sync value before wake up RT thread
	CurrentFrameRT = CurrentFrameGT;
	FrameNumberRT = GFrameNumber;
	bVsyncRT = ConfigInterface->PresentationSetting().bVsync;
	bIsSwapChainResetRT = bIsSwapChainResetGT;
	bEnableAsyncComputeRT = bEnableAsyncComputeGT;
	bIsRenderingEnabledRT = CurrentScene->GetMainCamera() && CurrentScene->GetMainCamera()->IsEnabled();
	bIsSkyLightEnabledRT = GetCurrentSkyCube() != nullptr;
	bHasRefractionMaterialRT = bHasRefractionMaterialGT;
	FrontmostRefractionIndexRT = FrontmostRefractionIndexGT;
	RTCullingDistanceRT = ConfigInterface->RenderingSetting().RTCullingRadius;

	if (SkyMeshRT == nullptr)
	{
		SkyMeshRT = AssetManagerInterface->GetMesh("UHMesh_Cube");
	}

	// wake render thread
	RenderThread->WakeThread();

#if WITH_EDITOR
	// only wait RT in editor, shipped build will wait in somewhere else by calling WaitPreviousRenderTask()
	RenderThread->WaitTask();
#endif

	// advance frame after rendering is done
	CurrentFrameGT = (CurrentFrameGT + 1) % GMaxFrameInFlight;
}

void UHDeferredShadingRenderer::WaitPreviousRenderTask()
{
	// wait render thread done
	RenderThread->WaitTask();
}

#if WITH_EDITOR

void UHDeferredShadingRenderer::SetDebugViewIndex(int32_t Idx)
{
	DebugViewIndex = Idx;
	if (DebugViewIndex > 0)
	{
		// wait before new binding
		GraphicInterface->WaitGPU();

		uint32_t ViewMipLevel = 0;
		DebugViewShader->GetDebugViewData()->UploadAllData(&ViewMipLevel);
		DebugViewShader->BindParameters();

		bDrawDebugViewRT = true;
		UHRenderTexture* Buffers[] = { nullptr, GSceneDiffuse, GSceneNormal, GSceneMaterial, GSceneDepth, GMotionVectorRT, GSceneMip, GRTShadowResult };
		if (Buffers[DebugViewIndex] != nullptr)
		{
			DebugViewShader->BindImage(Buffers[DebugViewIndex], 1);
		}
		else
		{
			bDrawDebugViewRT = false;
		}
	}
}

void UHDeferredShadingRenderer::SetEditorDelta(uint32_t InWidthDelta, uint32_t InHeightDelta)
{
	EditorWidthDelta = InWidthDelta;
	EditorHeightDelta = InHeightDelta;
}

float UHDeferredShadingRenderer::GetRenderThreadTime() const
{
	return RenderThreadTime;
}

int32_t UHDeferredShadingRenderer::GetDrawCallCount() const
{
	return DrawCalls;
}

float* UHDeferredShadingRenderer::GetGPUTimes()
{
	return &GPUTimes[0];
}

#endif

void UHDeferredShadingRenderer::UploadDataBuffers()
{
	UHCameraComponent* CurrentCamera = CurrentScene->GetMainCamera();
	if (!CurrentCamera)
	{
		return;
	}

	if (!CurrentCamera->IsEnabled())
	{
		return;
	}

	// setup system constants and upload
	SystemConstantsCPU.UHViewProj = CurrentCamera->GetViewProjMatrix();
	SystemConstantsCPU.UHViewProjInv = CurrentCamera->GetInvViewProjMatrix();
	SystemConstantsCPU.UHPrevViewProj_NonJittered = CurrentCamera->GetPrevViewProjMatrixNonJittered();
	SystemConstantsCPU.UHViewProj_NonJittered = CurrentCamera->GetViewProjMatrixNonJittered();
	SystemConstantsCPU.UHViewProjInv_NonJittered = CurrentCamera->GetInvViewProjMatrixNonJittered();
	SystemConstantsCPU.UHView = CurrentCamera->GetViewMatrix();
	SystemConstantsCPU.UHProjInv = CurrentCamera->GetInvProjMatrix();
	SystemConstantsCPU.UHProjInv_NonJittered = CurrentCamera->GetInvProjMatrixNonJittered();

	SystemConstantsCPU.UHResolution.x = static_cast<float>(RenderResolution.width);
	SystemConstantsCPU.UHResolution.y = static_cast<float>(RenderResolution.height);
	SystemConstantsCPU.UHResolution.z = 1.0f / SystemConstantsCPU.UHResolution.x;
	SystemConstantsCPU.UHResolution.w = 1.0f / SystemConstantsCPU.UHResolution.y;

	SystemConstantsCPU.UHCameraPos = CurrentCamera->GetPosition();
	SystemConstantsCPU.UHCameraDir = CurrentCamera->GetForward();
	SystemConstantsCPU.UHNumDirLights = static_cast<uint32_t>(CurrentScene->GetDirLightCount());
	SystemConstantsCPU.UHNumPointLights = static_cast<uint32_t>(CurrentScene->GetPointLightCount());
	SystemConstantsCPU.UHNumSpotLights = static_cast<uint32_t>(CurrentScene->GetSpotLightCount());
	SystemConstantsCPU.UHMaxPointLightPerTile = MaxPointLightPerTile;
	SystemConstantsCPU.UHMaxSpotLightPerTile = MaxSpotLightPerTile;

	uint32_t Dummy;
	GetLightCullingTileCount(SystemConstantsCPU.UHLightTileCountX, Dummy);

	if (ConfigInterface->RenderingSetting().bTemporalAA)
	{
		XMFLOAT4 Offset = CurrentCamera->GetJitterOffset();
		SystemConstantsCPU.JitterOffsetX = Offset.x;
		SystemConstantsCPU.JitterOffsetY = Offset.y;
		SystemConstantsCPU.JitterScaleMin = Offset.z;
		SystemConstantsCPU.JitterScaleFactor = Offset.w;
	}

	// set sky light data
	UHSkyLightComponent* SkyLight = CurrentScene->GetSkyLight();
	SystemConstantsCPU.UHAmbientSky = (SkyLight && SkyLight->IsEnabled()) ? SkyLight->GetSkyColor() * SkyLight->GetSkyIntensity() : XMFLOAT3();
	SystemConstantsCPU.UHAmbientGround = (SkyLight && SkyLight->IsEnabled()) ? SkyLight->GetGroundColor() * SkyLight->GetGroundIntensity() : XMFLOAT3();

	SystemConstantsCPU.RTShadowResolution.x = static_cast<float>(RTShadowExtent.width);
	SystemConstantsCPU.RTShadowResolution.y = static_cast<float>(RTShadowExtent.height);
	SystemConstantsCPU.RTShadowResolution.z = 1.0f / SystemConstantsCPU.RTShadowResolution.x;
	SystemConstantsCPU.RTShadowResolution.w = 1.0f / SystemConstantsCPU.RTShadowResolution.y;

	SystemConstantsCPU.UHNumRTInstances = RTInstanceCount;
	SystemConstantsCPU.UHFrameNumber = GFrameNumber;
	SystemConstantsCPU.UHPrepassDepthEnabled = bEnableDepthPrePass;
	SystemConstantsCPU.UHEnvironmentCubeEnabled = GetCurrentSkyCube() != nullptr;
	SystemConstantsCPU.UHDirectionalShadowRayTMax = ConfigInterface->RenderingSetting().RTShadowTMax;

	GSystemConstantBuffer[CurrentFrameGT]->UploadAllData(&SystemConstantsCPU);

	// upload object constants, only update CPU value if transform is changed
	if (CurrentScene->GetAllRendererCount() > 0)
	{
		const std::vector<UHMeshRendererComponent*>& Renderers = CurrentScene->GetAllRenderers();
		for (size_t Idx = 0; Idx < Renderers.size(); Idx++)
		{
			// update CPU constants when the frame is dirty
			// the dirty flag is marked in UHMeshRendererComponent::Update()
			UHMeshRendererComponent* Renderer = Renderers[Idx];
			if (Renderer->IsRenderDirty(CurrentFrameGT))
			{
				ObjectConstantsCPU[Idx] = Renderer->GetConstants();
				Renderer->SetRenderDirty(false, CurrentFrameGT);
			}

			// copy material data only when it's dirty
			UHMaterial* Mat = Renderer->GetMaterial();
			if (Mat->IsRenderDirty(CurrentFrameGT))
			{
				UHSystemMaterialData MaterialData{};
				MaterialData.DefaultSamplerIndex = DefaultSamplerIndex;
				MaterialData.InEnvCube = GetCurrentSkyCube();
				MaterialData.RefractionClearIndex = GRefractionClearIndex;
				MaterialData.RefractionBlurredIndex = GRefractionBlurredIndex;

				Mat->UploadMaterialData(CurrentFrameGT, MaterialData);
				Mat->SetRenderDirty(false, CurrentFrameGT);
			}
		}
		GObjectConstantBuffer[CurrentFrameGT]->UploadAllData(ObjectConstantsCPU.data());
	}

	// upload directional light data
	if (CurrentScene->GetDirLightCount() > 0)
	{
		const std::vector<UHDirectionalLightComponent*>& DirLights = CurrentScene->GetDirLights();
		for (size_t Idx = 0; Idx < DirLights.size(); Idx++)
		{
			UHDirectionalLightComponent* Light = DirLights[Idx];
			if (Light->IsRenderDirty(CurrentFrameGT))
			{
				DirLightConstantsCPU[Idx] = Light->GetConstants();
				Light->SetRenderDirty(false, CurrentFrameGT);
			}
		}
		GDirectionalLightBuffer[CurrentFrameGT]->UploadAllData(DirLightConstantsCPU.data());
	}

	// upload point light data
	if (CurrentScene->GetPointLightCount() > 0)
	{
		const std::vector<UHPointLightComponent*>& PointLights = CurrentScene->GetPointLights();
		for (size_t Idx = 0; Idx < PointLights.size(); Idx++)
		{
			UHPointLightComponent* Light = PointLights[Idx];
			if (Light->IsRenderDirty(CurrentFrameGT))
			{
				PointLightConstantsCPU[Idx] = Light->GetConstants();
				Light->SetRenderDirty(false, CurrentFrameGT);
			}
		}
		GPointLightBuffer[CurrentFrameGT]->UploadAllData(PointLightConstantsCPU.data());
	}

	// upload spot light data
	if (CurrentScene->GetSpotLightCount() > 0)
	{
		const std::vector<UHSpotLightComponent*>& SpotLights = CurrentScene->GetSpotLights();
		for (size_t Idx = 0; Idx < SpotLights.size(); Idx++)
		{
			UHSpotLightComponent* Light = SpotLights[Idx];
			if (Light->IsRenderDirty(CurrentFrameGT))
			{
				SpotLightConstantsCPU[Idx] = Light->GetConstants();
				Light->SetRenderDirty(false, CurrentFrameGT);
			}
		}
		GSpotLightBuffer[CurrentFrameGT]->UploadAllData(SpotLightConstantsCPU.data());
	}

	// upload SH9 data, for now use the 4th mip slice in it
	UHSphericalHarmonicConstants SH9Constant{};
	SH9Constant.MipLevel = 4;
	SH9Constant.Weight = 4.0f * G_PI / 64.0f;
	SH9Shader->GetSH9Constants(CurrentFrameGT)->UploadAllData(&SH9Constant);

	// upload tone map data
	uint32_t IsHDR = GraphicInterface->IsHDRAvailable();
	ToneMapShader->GetToneMapData(CurrentFrameGT)->UploadAllData(&IsHDR);
}

void UHDeferredShadingRenderer::FrustumCulling()
{
	const UHCameraComponent* CurrentCamera = CurrentScene->GetMainCamera();
	if (!CurrentCamera)
	{
		return;
	}

	if (!CurrentCamera->IsEnabled())
	{
		return;
	}

	// wake frustum culling task
	ParallelTask = UHParallelTask::FrustumCullingTask;
	for (int32_t I = 0; I < NumWorkerThreads; I++)
	{
		WorkerThreads[I]->WakeThread();
	}

	for (int32_t I = 0; I < NumWorkerThreads; I++)
	{
		WorkerThreads[I]->WaitTask();
	}
}

void UHDeferredShadingRenderer::FrustumCullingTask(int32_t ThreadIdx)
{
	const UHCameraComponent* CurrentCamera = CurrentScene->GetMainCamera();
	const BoundingFrustum& CameraFrustum = CurrentCamera->GetBoundingFrustum();
	const std::vector<UHMeshRendererComponent*>& Renderers = CurrentScene->GetAllRenderers();
	const XMFLOAT3 CameraPos = CurrentCamera->GetPosition();

	const int32_t MaxCount = static_cast<int32_t>(Renderers.size());
	const int32_t RendererCount = (MaxCount + NumWorkerThreads) / NumWorkerThreads;
	const int32_t StartIdx = std::min(RendererCount * ThreadIdx, MaxCount);
	const int32_t EndIdx = (ThreadIdx == NumWorkerThreads - 1) ? MaxCount : std::min(StartIdx + RendererCount, MaxCount);

	for (int32_t Idx = StartIdx; Idx < EndIdx; Idx++)
	{
		// skip skybox renderer
		UHMeshRendererComponent* Renderer = Renderers[Idx];
		if (Renderer->GetMaterial()->GetMaterialUsages().bIsSkybox)
		{
			continue;
		}

		const BoundingBox& RendererBound = Renderer->GetRendererBound();
		const bool bVisible = (CameraFrustum.Contains(RendererBound) != DirectX::DISJOINT);
		Renderer->SetVisible(bVisible);

		// also calculate the square distance to current camera for later use
		Renderer->CalculateSquareDistanceTo(CameraPos);
	}
}

void UHDeferredShadingRenderer::CollectVisibleRenderer()
{
	// recommend to reserve capacity during initialization
	OpaquesToRender.clear();
	const std::vector<UHMeshRendererComponent*>& OpaqueRenderers = CurrentScene->GetOpaqueRenderers();
	for (UHMeshRendererComponent* Renderer : OpaqueRenderers)
	{
		if (Renderer->IsVisible())
		{
			OpaquesToRender.push_back(Renderer);
		}
	}

	TranslucentsToRender.clear();
	const std::vector<UHMeshRendererComponent*>& TranslucentRenderers = CurrentScene->GetTranslucentRenderers();
	for (UHMeshRendererComponent* Renderer : TranslucentRenderers)
	{
		if (Renderer->IsVisible())
		{
			TranslucentsToRender.push_back(Renderer);
		}
	}
}

void UHDeferredShadingRenderer::SortRenderer()
{
	const UHCameraComponent* CurrentCamera = CurrentScene->GetMainCamera();
	if (!CurrentCamera)
	{
		return;
	}

	if (!CurrentCamera->IsEnabled())
	{
		return;
	}

	// sort front-to-back
	XMFLOAT3 CameraPos = CurrentCamera->GetPosition();
	std::sort(OpaquesToRender.begin(), OpaquesToRender.end(), [&CameraPos](UHMeshRendererComponent* A, UHMeshRendererComponent* B)
		{
			return A->GetSquareDistanceToMainCam() < B->GetSquareDistanceToMainCam();
		});

	// sort back-to-front for translucents
	std::sort(TranslucentsToRender.begin(), TranslucentsToRender.end(), [&CameraPos](UHMeshRendererComponent* A, UHMeshRendererComponent* B)
		{
			return A->GetSquareDistanceToMainCam() > B->GetSquareDistanceToMainCam();
		});

	// find the frontmost translucent object that does refraction
	// The rendering later will do a workflow: Draw background translucent -> Screenshot the result -> Draw Foreground translucent
	// So at least the frontmost refraction object can refract some translucent objects behind it!
	bHasRefractionMaterialGT = false;

	FrontmostRefractionIndexGT = UHINDEXNONE;
	for (size_t Idx = 0; Idx < TranslucentsToRender.size(); Idx++)
	{
		if (const UHMaterial* Mat = TranslucentsToRender[Idx]->GetMaterial())
		{
			if (Mat->GetMaterialUsages().bUseRefraction)
			{
				FrontmostRefractionIndexGT = static_cast<int32_t>(Idx);
			}
		}
	}
	bHasRefractionMaterialGT = (FrontmostRefractionIndexGT != UHINDEXNONE);
}

void UHDeferredShadingRenderer::GetLightCullingTileCount(uint32_t& TileCountX, uint32_t& TileCountY)
{
	// safely round up the tile counts, doing culling at half resolution
	TileCountX = ((RenderResolution.width >> 1) + LightCullingTileSize) / LightCullingTileSize;
	TileCountY = ((RenderResolution.height >> 1) + LightCullingTileSize) / LightCullingTileSize;
}

UHTextureCube* UHDeferredShadingRenderer::GetCurrentSkyCube() const
{
	UHTextureCube* CurrSkyCube = nullptr;
	if (UHSkyLightComponent* SkyLightComp = CurrentScene->GetSkyLight())
	{
		if (SkyLightComp->IsEnabled())
		{
			CurrSkyCube = SkyLightComp->GetCubemap();
		}
	}

	// set fallback cube if it's not available
	if (CurrSkyCube == nullptr)
	{
		CurrSkyCube = GBlackCube;
	}

	return CurrSkyCube;
}

void UHDeferredShadingRenderer::RenderThreadLoop()
{
	/** Render steps **/
	// Wait for the previous frame to finish
	// Acquire an image from the swap chain
	// Record a command buffer which draws the scene onto that image
	// Submit the recorded command buffer
	// Present the swap chain image

	// hold a timer
	UHGameTimer RTTimer;
	UHProfiler RenderThreadProfile(&RTTimer);
	bool bIsPresentedPreviously = false;

	while (true)
	{
		// wait until main thread notify
		RenderThread->WaitNotify();

		if (RenderThread->IsTermindate())
		{
			break;
		}

		// prepare graphic builder
		UHRenderBuilder SceneRenderBuilder(GraphicInterface, SceneRenderQueue.CommandBuffers[CurrentFrameRT]);
		uint32_t PresentIndex;
		{
			UHProfilerScope Profiler(&RenderThreadProfile);

			// collect worker bundle for this frame
			if (bIsRenderingEnabledRT)
			{
				DepthParallelSubmitter.CollectCurrentFrameRTBundle(CurrentFrameRT);
				BaseParallelSubmitter.CollectCurrentFrameRTBundle(CurrentFrameRT);
				MotionOpaqueParallelSubmitter.CollectCurrentFrameRTBundle(CurrentFrameRT);
				MotionTranslucentParallelSubmitter.CollectCurrentFrameRTBundle(CurrentFrameRT);
				TranslucentParallelSubmitter.CollectCurrentFrameRTBundle(CurrentFrameRT);
			}

			if (bEnableAsyncComputeRT)
			{
				// ****************************** start async compute queue
				// kick off the tasks that be executed on compute queue, note that not every tasks are suitable for async
				// the golden rule of utilizing async compute queue:
				// (1) Must be compute/raytracing shader..ofc
				// (2) Must not access to the same resource at the same time
				UHRenderBuilder AsyncComputeBuilder(GraphicInterface, AsyncComputeQueue.CommandBuffers[CurrentFrameRT], true);
				AsyncComputeBuilder.WaitFence(AsyncComputeQueue.Fences[CurrentFrameRT]);
				AsyncComputeBuilder.ResetFence(AsyncComputeQueue.Fences[CurrentFrameRT]);
				AsyncComputeBuilder.BeginCommandBuffer();
				GraphicInterface->BeginCmdDebug(AsyncComputeBuilder.GetCmdList(), "Executing Async Compute");

				if (bIsRenderingEnabledRT)
				{
					BuildTopLevelAS(AsyncComputeBuilder);
					GenerateSH9Pass(AsyncComputeBuilder);
				}

				GraphicInterface->EndCmdDebug(AsyncComputeBuilder.GetCmdList());
				AsyncComputeBuilder.EndCommandBuffer();
				AsyncComputeBuilder.ExecuteCmd(AsyncComputeQueue.Queue, AsyncComputeQueue.Fences[CurrentFrameRT], nullptr, AsyncComputeQueue.FinishedSemaphores[CurrentFrameRT]);
				// ****************************** end async compute queue
			}

			// ****************************** start scene rendering
			// similar to D3D12 fence wait/reset
			SceneRenderBuilder.WaitFence(SceneRenderQueue.Fences[CurrentFrameRT]);
			SceneRenderBuilder.ResetFence(SceneRenderQueue.Fences[CurrentFrameRT]);

			// begin command buffer, it will reset command buffer inline
			SceneRenderBuilder.BeginCommandBuffer();
			GraphicInterface->BeginCmdDebug(SceneRenderBuilder.GetCmdList(), "Drawing UHDeferredShadingRenderer");

			if (bIsRenderingEnabledRT)
			{
				// first-chance resource barriers
				if (!bIsPresentedPreviously || bIsSwapChainResetRT)
				{
					SceneRenderBuilder.PushResourceBarrier(UHImageBarrier(GOpaqueSceneResult, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
					SceneRenderBuilder.PushResourceBarrier(UHImageBarrier(GQuarterBlurredScene, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
					SceneRenderBuilder.FlushResourceBarrier();
				}

				RenderDepthPrePass(SceneRenderBuilder);
				RenderBasePass(SceneRenderBuilder);
				RenderMotionPass(SceneRenderBuilder);
				DownsampleDepthPass(SceneRenderBuilder);
				if (!bEnableAsyncComputeRT)
				{
					BuildTopLevelAS(SceneRenderBuilder);
					GenerateSH9Pass(SceneRenderBuilder);
				}
				DispatchLightCulling(SceneRenderBuilder);
				DispatchRayShadowPass(SceneRenderBuilder);
				RenderLightPass(SceneRenderBuilder);
				RenderSkyPass(SceneRenderBuilder);
				PreTranslucentPass(SceneRenderBuilder);
				RenderTranslucentPass(SceneRenderBuilder);
				RenderPostProcessing(SceneRenderBuilder);
			}

			// blit scene to swap chain
			PresentIndex = RenderSceneToSwapChain(SceneRenderBuilder);

		#if WITH_EDITOR
			// get GPU times
			for (int32_t Idx = 0; Idx < UHRenderPassMax; Idx++)
			{
				GPUTimes[Idx] = GPUTimeQueries[Idx]->GetTimeStamp(SceneRenderBuilder.GetCmdList());
			}
		#endif

			GraphicInterface->EndCmdDebug(SceneRenderBuilder.GetCmdList());
			SceneRenderBuilder.EndCommandBuffer();

			// wait the previous async queue is done (that means async compute queue always advanced one frame more than graphic)
			// also needs to wait the swap chain is ready
			std::vector<VkSemaphore> WaitSemaphore;
			std::vector<VkPipelineStageFlags> WaitStages;

			if (bEnableAsyncComputeRT)
			{
				WaitSemaphore.push_back(AsyncComputeQueue.FinishedSemaphores[CurrentFrameRT]);
				WaitStages.push_back(VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
			}
			WaitSemaphore.push_back(SceneRenderQueue.WaitingSemaphores[CurrentFrameRT]);
			WaitStages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

			SceneRenderBuilder.ExecuteCmd(SceneRenderQueue.Queue, SceneRenderQueue.Fences[CurrentFrameRT], WaitSemaphore, WaitStages, SceneRenderQueue.FinishedSemaphores[CurrentFrameRT]);
			// ****************************** end scene rendering
		}

	#if WITH_EDITOR
		// profile ends before Present() call, since it contains vsync time
		RenderThreadTime = RenderThreadProfile.GetDiff() * 1000.0f;
		DrawCalls = SceneRenderBuilder.DrawCalls;
	#endif

		// wait until the previous presentation is done
		if (bIsPresentedPreviously && !bIsSwapChainResetRT)
		{
			SceneRenderBuilder.WaitFence(SceneRenderQueue.Fences[1 - CurrentFrameRT]);
		}

		// present
		bIsResetNeededShared = !SceneRenderBuilder.Present(GraphicInterface->GetSwapChain(), SceneRenderQueue.Queue, SceneRenderQueue.FinishedSemaphores[CurrentFrameRT], PresentIndex);
		bIsPresentedPreviously = true;

		// tell main thread to continue
		RenderThread->NotifyTaskDone();
	}
}

void UHDeferredShadingRenderer::WorkerThreadLoop(int32_t ThreadIdx)
{
	/** Worker steps **/
	// Specify the task id before wake it
	// doing the task
	// notify finished

	while (true)
	{
		WorkerThreads[ThreadIdx]->WaitNotify();

		if (WorkerThreads[ThreadIdx]->IsTermindate())
		{
			break;
		}

		switch (ParallelTask)
		{
		case UHParallelTask::FrustumCullingTask:
			FrustumCullingTask(ThreadIdx);
			break;

		case UHParallelTask::DepthPassTask:
			DepthPassTask(ThreadIdx);
			break;

		case UHParallelTask::BasePassTask:
			BasePassTask(ThreadIdx);
			break;

		case UHParallelTask::MotionOpaqueTask:
			MotionOpaqueTask(ThreadIdx);
			break;

		case UHParallelTask::MotionTranslucentTask:
			MotionTranslucentTask(ThreadIdx);
			break;

		case UHParallelTask::TranslucentBgPassTask:
			TranslucentPassTask(ThreadIdx);
			break;

		default:
			break;
		};

		WorkerThreads[ThreadIdx]->NotifyTaskDone();
	}
}