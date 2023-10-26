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

	// resize buffers only otherwise
	ReleaseRenderPassObjects(true);
	RelaseRenderingBuffers();
	CreateRenderingBuffers();

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
	bParallelSubmissionRT = ConfigInterface->RenderingSetting().bParallelSubmission;
	CurrentFrameRT = CurrentFrameGT;
	FrameNumberRT = GFrameNumber;
	bVsyncRT = ConfigInterface->PresentationSetting().bVsync;
	bIsSwapChainResetRT = bIsSwapChainResetGT;
	bEnableAsyncComputeRT = bEnableAsyncComputeGT;

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
		DebugViewData->UploadAllData(&ViewMipLevel);
		for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
		{
			DebugViewShader->BindConstant(DebugViewData, 0, Idx);
		}

		UHRenderTexture* Buffers[] = { nullptr, SceneDiffuse, SceneNormal, SceneMaterial, SceneDepth, MotionVectorRT, SceneMip, RTShadowResult };
		DebugViewShader->BindImage(Buffers[DebugViewIndex], 1);
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

std::array<float, UHRenderPassTypes::UHRenderPassMax> UHDeferredShadingRenderer::GetGPUTimes() const
{
	return GPUTimes;
}

#endif

void UHDeferredShadingRenderer::UploadDataBuffers()
{
	UHCameraComponent* CurrentCamera = CurrentScene->GetMainCamera();
	if (!CurrentCamera)
	{
		// don't update without a camera
		UHE_LOG(L"Camera is not set!\n");
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
	SystemConstantsCPU.UHAmbientSky = SkyLight->GetSkyColor() * SkyLight->GetSkyIntensity();
	SystemConstantsCPU.UHAmbientGround = SkyLight->GetGroundColor() * SkyLight->GetGroundIntensity();

	SystemConstantsCPU.UHShadowResolution.x = static_cast<float>(RTShadowExtent.width);
	SystemConstantsCPU.UHShadowResolution.y = static_cast<float>(RTShadowExtent.height);
	SystemConstantsCPU.UHShadowResolution.z = 1.0f / SystemConstantsCPU.UHShadowResolution.x;
	SystemConstantsCPU.UHShadowResolution.w = 1.0f / SystemConstantsCPU.UHShadowResolution.y;

	SystemConstantsCPU.UHNumRTInstances = RTInstanceCount;

	SystemConstantsGPU[CurrentFrameGT]->UploadAllData(&SystemConstantsCPU);

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
				Mat->UploadMaterialData(CurrentFrameGT, DefaultSamplerIndex);
				Mat->SetRenderDirty(false, CurrentFrameGT);
			}
		}
		ObjectConstantsGPU[CurrentFrameGT]->UploadAllData(ObjectConstantsCPU.data());
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
		DirectionalLightBuffer[CurrentFrameGT]->UploadAllData(DirLightConstantsCPU.data());
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
		PointLightBuffer[CurrentFrameGT]->UploadAllData(PointLightConstantsCPU.data());
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
		SpotLightBuffer[CurrentFrameGT]->UploadAllData(SpotLightConstantsCPU.data());
	}

	// upload tone map data
	uint32_t IsHDR = GraphicInterface->IsHDRSupported() && ConfigInterface->RenderingSetting().bEnableHDR;
	ToneMapData[CurrentFrameGT]->UploadAllData(&IsHDR);
}

void UHDeferredShadingRenderer::FrustumCulling()
{
	const UHCameraComponent* CurrentCamera = CurrentScene->GetMainCamera();
	if (!CurrentCamera)
	{
		// don't cull without a camera
		UHE_LOG(L"Camera is not set!\n");
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

	const int32_t MaxCount = static_cast<int32_t>(Renderers.size());
	const int32_t RendererCount = (MaxCount + NumWorkerThreads) / NumWorkerThreads;
	const int32_t StartIdx = std::min(RendererCount * ThreadIdx, MaxCount);
	const int32_t EndIdx = (ThreadIdx == NumWorkerThreads - 1) ? MaxCount : std::min(StartIdx + RendererCount, MaxCount);

	for (int32_t Idx = StartIdx; Idx < EndIdx; Idx++)
	{
		// skip skybox renderer
		UHMeshRendererComponent* Renderer = Renderers[Idx];
		if (Renderer->GetMaterial()->IsSkybox())
		{
			continue;
		}

		const BoundingBox& RendererBound = Renderer->GetRendererBound();
		const bool bVisible = (CameraFrustum.Contains(RendererBound) != DirectX::DISJOINT);
		Renderer->SetVisible(bVisible);
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
		// don't cull without a camera
		UHE_LOG(L"Camera is not set!\n");
		return;
	}

	// wake sorting opaque task
	ParallelTask = UHParallelTask::SortingOpaqueTask;
	for (int32_t I = 0; I < NumWorkerThreads; I++)
	{
		WorkerThreads[I]->WakeThread();
	}

	for (int32_t I = 0; I < NumWorkerThreads; I++)
	{
		WorkerThreads[I]->WaitTask();
	}

	// sort back-to-front for translucents, this unfortunately can't be parallel unless going OIT
	XMFLOAT3 CameraPos = CurrentCamera->GetPosition();
	std::sort(TranslucentsToRender.begin(), TranslucentsToRender.end(), [&CameraPos](UHMeshRendererComponent* A, UHMeshRendererComponent* B)
		{
			XMFLOAT3 ZA = A->GetRendererBound().Center;
			XMFLOAT3 ZB = B->GetRendererBound().Center;

			return MathHelpers::VectorDistanceSqr(ZA, CameraPos) > MathHelpers::VectorDistanceSqr(ZB, CameraPos);
		});
}

void UHDeferredShadingRenderer::GetLightCullingTileCount(uint32_t& TileCountX, uint32_t& TileCountY)
{
	// safely round up the tile counts, doing culling at half resolution
	TileCountX = ((RenderResolution.width >> 1) + LightCullingTileSize) / LightCullingTileSize;
	TileCountY = ((RenderResolution.height >> 1) + LightCullingTileSize) / LightCullingTileSize;
}

void UHDeferredShadingRenderer::SortingOpaqueTask(int32_t ThreadIdx)
{
	const UHCameraComponent* CurrentCamera = CurrentScene->GetMainCamera();
	XMFLOAT3 CameraPos = CurrentCamera->GetPosition();

	// sort opaques first
	const int32_t MaxCount = static_cast<int32_t>(OpaquesToRender.size());
	const int32_t RendererCount = (MaxCount + NumWorkerThreads) / NumWorkerThreads;
	const int32_t StartIdx = std::min(RendererCount * ThreadIdx, MaxCount);
	const int32_t EndIdx = (ThreadIdx == NumWorkerThreads - 1) ? MaxCount : std::min(StartIdx + RendererCount, MaxCount);

	std::sort(OpaquesToRender.begin() + StartIdx, OpaquesToRender.begin() + EndIdx, [&CameraPos](UHMeshRendererComponent* A, UHMeshRendererComponent* B)
		{
			// sort front-to-back for opaque, rough z sort first
			XMFLOAT3 ZA = A->GetRendererBound().Center;
			XMFLOAT3 ZB = B->GetRendererBound().Center;

			return MathHelpers::VectorDistanceSqr(ZA, CameraPos) < MathHelpers::VectorDistanceSqr(ZB, CameraPos);
		});

	std::sort(OpaquesToRender.begin() + StartIdx, OpaquesToRender.begin() + EndIdx, [&CameraPos](UHMeshRendererComponent* A, UHMeshRendererComponent* B)
		{
			// then sort by material id to reduce state change
			return A->GetMaterial()->GetId() < B->GetMaterial()->GetId();
		});

	// so this task simply divides opaque list to a few group and sort them
	// this is a tradeoff, I lose the perfect sorting but gaining perf
}

void UHDeferredShadingRenderer::RenderThreadLoop()
{
	/** Render steps **/
	// Wait for the previous frame to finish
	// Acquire an image from the swap chain
	// Record a command buffer which draws the scene onto that image
	// Submit the recorded command buffer
	// Present the swap chain image
	UHE_LOG(L"Render thread created.\n");

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
			if (bParallelSubmissionRT)
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

				BuildTopLevelAS(AsyncComputeBuilder);

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

			RenderDepthPrePass(SceneRenderBuilder);
			RenderBasePass(SceneRenderBuilder);
			RenderMotionPass(SceneRenderBuilder);
			if (!bEnableAsyncComputeRT)
			{
				BuildTopLevelAS(SceneRenderBuilder);
			}
			DispatchLightCulling(SceneRenderBuilder);
			DispatchRayShadowPass(SceneRenderBuilder);
			RenderLightPass(SceneRenderBuilder);
			RenderSkyPass(SceneRenderBuilder);
			RenderTranslucentPass(SceneRenderBuilder);
			RenderPostProcessing(SceneRenderBuilder);

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
			std::vector<VkSemaphore> WaitSemaphore = { SceneRenderQueue.WaitingSemaphores[CurrentFrameRT] };
			if (bIsPresentedPreviously && bEnableAsyncComputeRT)
			{
				WaitSemaphore.push_back(AsyncComputeQueue.FinishedSemaphores[1 - CurrentFrameRT]);
			}

			std::vector<VkPipelineStageFlags> WaitStages = { VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
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
			if (!GraphicInterface->IsPresentWaitSupported())
			{
				SceneRenderBuilder.WaitFence(SceneRenderQueue.Fences[1 - CurrentFrameRT]);
			}
			else if (GVkWaitForPresentKHR(GraphicInterface->GetLogicalDevice(), GraphicInterface->GetSwapChain(), FrameNumberRT - 1, 100000000) != VK_SUCCESS)
			{
				UHE_LOG("Waiting for presentation failed!\n");
			}
		}

		// present
		bIsResetNeededShared = !SceneRenderBuilder.Present(GraphicInterface->GetSwapChain(), SceneRenderQueue.Queue, SceneRenderQueue.FinishedSemaphores[CurrentFrameRT], PresentIndex, FrameNumberRT);
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
	UHE_LOG(L"Worker thread " + std::to_wstring(ThreadIdx) + L" created.\n");

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

		case UHParallelTask::SortingOpaqueTask:
			SortingOpaqueTask(ThreadIdx);
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

		case UHParallelTask::TranslucentPassTask:
			TranslucentPassTask(ThreadIdx);
			break;

		default:
			break;
		};

		WorkerThreads[ThreadIdx]->NotifyTaskDone();
	}
}