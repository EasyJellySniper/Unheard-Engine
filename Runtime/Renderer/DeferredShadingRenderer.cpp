#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::SetCurrentScene(UHScene* InScene)
{
	CurrentScene = InScene;
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

	// wake render thread
	std::unique_lock<std::mutex> Lock(RenderMutex);
	bIsThisFrameRenderedShared = false;
	WaitRenderThread.notify_one();

	// wait render thread done
	RenderThreadFinished.wait(Lock, [this] {return bIsThisFrameRenderedShared; });
}

#if WITH_DEBUG

void UHDeferredShadingRenderer::SetDebugViewIndex(int32_t Idx)
{
	DebugViewIndex = Idx;
	if (DebugViewIndex > 0)
	{
		// wait before new binding
		GraphicInterface->WaitGPU();
		UHRenderTexture* Buffers[] = { nullptr, SceneDiffuse,SceneNormal,SceneMaterial, SceneDepth, MotionVectorRT, SceneMip, RTShadowResult };
		DebugViewShader.BindImage(Buffers[DebugViewIndex], 0);
	}
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

	SystemConstantsCPU.UHResolution.x = static_cast<float>(RenderResolution.width);
	SystemConstantsCPU.UHResolution.y = static_cast<float>(RenderResolution.height);
	SystemConstantsCPU.UHResolution.z = 1.0f / SystemConstantsCPU.UHResolution.x;
	SystemConstantsCPU.UHResolution.w = 1.0f / SystemConstantsCPU.UHResolution.y;

	SystemConstantsCPU.UHCameraPos = CurrentCamera->GetPosition();
	SystemConstantsCPU.UHCameraDir = CurrentCamera->GetForward();
	SystemConstantsCPU.NumDirLights = static_cast<uint32_t>(CurrentScene->GetDirLightCount());

	if (ConfigInterface->RenderingSetting().bTemporalAA)
	{
		XMFLOAT2 Offset = XMFLOAT2(MathHelpers::Halton(GFrameNumber & 511, 2), MathHelpers::Halton(GFrameNumber & 511, 3));
		SystemConstantsCPU.JitterOffsetX = Offset.x * SystemConstantsCPU.UHResolution.z;
		SystemConstantsCPU.JitterOffsetY = Offset.y * SystemConstantsCPU.UHResolution.w;
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

	SystemConstantsGPU[CurrentFrame]->UploadAllData(&SystemConstantsCPU);

	// upload object constants, only upload if transform is changed
	const std::vector<UHMeshRendererComponent*>& Renderers = CurrentScene->GetAllRenderers();
	const std::vector<UHSampler*>& Samplers = GraphicInterface->GetSamplers();

	for (UHMeshRendererComponent* Renderer : Renderers)
	{
		// only copy when frame is dirty, clear dirty after copying
		// the dirty flag is marked in UHMeshRendererComponent::Update()
		if (Renderer->IsRenderDirty(CurrentFrame))
		{
			UHObjectConstants ObjCB = Renderer->GetConstants();

			// must copy to proper offset
			ObjectConstantsGPU[CurrentFrame]->UploadData(&ObjCB, Renderer->GetBufferDataIndex());

			Renderer->SetRenderDirty(false, CurrentFrame);
		}

		// copy material data
		UHMaterial* Mat = Renderer->GetMaterial();
		if (Mat->IsRenderDirty(CurrentFrame))
		{
			// transfer material CB
			UHMaterialConstants MatCB{};
			UHMaterialProperty MatProps = Mat->GetMaterialProps();

			MatCB.DiffuseColor = XMFLOAT4(MatProps.Diffuse.x, MatProps.Diffuse.y, MatProps.Diffuse.z, MatProps.Opacity);
			MatCB.EmissiveColor = XMFLOAT3(MatProps.Emissive.x, MatProps.Emissive.y, MatProps.Emissive.z) * MatProps.EmissiveIntensity;
			MatCB.AmbientOcclusion = MatProps.Occlusion;
			MatCB.Metallic = MatProps.Metallic;
			MatCB.Roughness = MatProps.Roughness;
			MatCB.SpecularColor = MatProps.Specular;
			MatCB.BumpScale = MatProps.BumpScale;
			MatCB.Cutoff = MatProps.Cutoff;
			MatCB.FresnelFactor = MatProps.FresnelFactor;
			MatCB.ReflectionFactor = MatProps.ReflectionFactor;

			if (UHTexture* EnvCube = Mat->GetTex(UHMaterialTextureType::SkyCube))
			{
				MatCB.EnvCubeMipMapCount = static_cast<float>(EnvCube->GetMipMapCount());
			}

			// set texture index
			MatCB.OpacityTexIndex = Mat->GetTextureIndex(UHMaterialTextureType::Opacity);

			// find sampler index in sampler pool
			if (UHSampler* Sampler = Mat->GetSampler(UHMaterialTextureType::Opacity))
			{
				MatCB.OpacitySamplerIndex = UHUtilities::FindIndex(Samplers, Sampler);
			}

			// must copy to proper offset
			MaterialConstantsGPU[CurrentFrame]->UploadData(&MatCB, Mat->GetBufferDataIndex());

			Mat->SetRenderDirty(false, CurrentFrame);
		}
	}

	// upload light data
	const std::vector<UHDirectionalLightComponent*>& DirLights = CurrentScene->GetDirLights();
	for (UHDirectionalLightComponent* Light : DirLights)
	{
		if (Light->IsRenderDirty(CurrentFrame))
		{
			UHDirectionalLightConstants DirLightC = Light->GetConstants();
			DirectionalLightBuffer[CurrentFrame]->UploadData(&DirLightC, Light->GetBufferDataIndex());
			Light->SetRenderDirty(false, CurrentFrame);
		}
	}
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

	const BoundingFrustum& CameraFrustum = CurrentCamera->GetBoundingFrustum();
	const std::vector<UHMeshRendererComponent*>& Renderers = CurrentScene->GetAllRenderers();

	for (UHMeshRendererComponent* Renderer : Renderers)
	{
		// skip skybox renderer
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

	XMFLOAT3 CameraPos = CurrentCamera->GetPosition();

	std::sort(OpaquesToRender.begin(), OpaquesToRender.end(), [&CameraPos](UHMeshRendererComponent* A, UHMeshRendererComponent* B)
		{
			// sort front-to-back for opaque, rough z sort
			XMFLOAT3 ZA = A->GetRendererBound().Center;
			XMFLOAT3 ZB = B->GetRendererBound().Center;

			return MathHelpers::VectorDistanceSqr(ZA , CameraPos) < MathHelpers::VectorDistanceSqr(ZB, CameraPos);
		});

	std::sort(TranslucentsToRender.begin(), TranslucentsToRender.end(), [&CameraPos](UHMeshRendererComponent* A, UHMeshRendererComponent* B)
		{
			XMFLOAT3 ZA = A->GetRendererBound().Center;
			XMFLOAT3 ZB = B->GetRendererBound().Center;

			return MathHelpers::VectorDistanceSqr(ZA, CameraPos) > MathHelpers::VectorDistanceSqr(ZB, CameraPos);
		});
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

#if WITH_DEBUG
	// hold a timer
	UHGameTimer RTTimer;
	UHProfiler RenderThreadProfile(&RTTimer);
#endif

	while (true)
	{
		// wait until main thread notify
		std::unique_lock<std::mutex> Lock(RenderMutex);
		WaitRenderThread.wait(Lock, [this] {return !bIsThisFrameRenderedShared; });

		if (bIsRenderThreadDoneShared)
		{
			break;
		}

	#if WITH_DEBUG
		RenderThreadProfile.Begin();
	#endif

		// prepare necessary variable
		UHGraphicBuilder GraphBuilder(GraphicInterface, MainCommandBuffers[CurrentFrame]);

		// similar to D3D12 fence wait/reset
		GraphBuilder.WaitFence(MainFences[CurrentFrame]);
		GraphBuilder.ResetFence(MainFences[CurrentFrame]);

		// begin command buffer, it will reset command buffer inline
		GraphBuilder.BeginCommandBuffer();
		GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Drawing UHDeferredShadingRenderer");

		// ****************************** start scene rendering
		BuildTopLevelAS(GraphBuilder);
		DispatchRayOcclusionTestPass(GraphBuilder);
		RenderDepthPrePass(GraphBuilder);
		RenderBasePass(GraphBuilder);
		DispatchRayShadowPass(GraphBuilder);
		RenderLightPass(GraphBuilder);
		RenderSkyPass(GraphBuilder);
		RenderMotionPass(GraphBuilder);
		RenderPostProcessing(GraphBuilder);

		// blit scene to swap chain
		uint32_t PresentIndex = RenderSceneToSwapChain(GraphBuilder);

	#if WITH_DEBUG
		// get GPU times
		for (int32_t Idx = 0; Idx < UHRenderPassMax; Idx++)
		{
			GPUTimes[Idx] = GPUTimeQueries[Idx]->GetTimeStamp(GraphBuilder.GetCmdList());
		}
	#endif

		// ****************************** end scene rendering
		GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
		GraphBuilder.EndCommandBuffer();
		GraphBuilder.ExecuteCmd(MainFences[CurrentFrame], SwapChainAvailableSemaphores[CurrentFrame], RenderFinishedSemaphores[CurrentFrame]);

	#if WITH_DEBUG
		// profile ends before Present() call, since it contains vsync time
		RenderThreadProfile.End();
		RenderThreadTime = RenderThreadProfile.GetDiff() * 1000.0f;
		DrawCalls = GraphBuilder.DrawCalls;
	#endif

		// present
		bIsResetNeededShared = !GraphBuilder.Present(RenderFinishedSemaphores[CurrentFrame], PresentIndex);

		// advance frame
		CurrentFrame = (CurrentFrame + 1) % GMaxFrameInFlight;

		// tell main thread to continue
		bIsThisFrameRenderedShared = true;
		Lock.unlock();
		RenderThreadFinished.notify_one();
	}
}