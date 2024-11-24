#include "DeferredShadingRenderer.h"

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

	ReleaseRayTracingBuffers();
	ResizeRayTracingBuffers(true);

	// need to rewrite descriptors after resize
	UpdateDescriptors();

	InitGaussianConstants();

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

	FrustumCulling();
	UploadDataBuffers();
	CollectVisibleRenderer();
	CollectMeshShaderInstance();
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
	bEnableAsyncComputeRT = ConfigInterface->RenderingSetting().bEnableAsyncCompute;
	bIsRenderingEnabledRT = CurrentScene->GetMainCamera() && CurrentScene->GetMainCamera()->IsEnabled();
	bIsSkyLightEnabledRT = GetCurrentSkyCube() != nullptr;
	bHasRefractionMaterialRT = bHasRefractionMaterialGT;
	bHDREnabledRT = GraphicInterface->IsHDRAvailable();
	RTCullingDistanceRT = ConfigInterface->RenderingSetting().RTCullingRadius;
	RTReflectionQualityRT = ConfigInterface->RenderingSetting().RTReflectionQuality;

	// at least make it 'toggleable' partially
	bIsRaytracingEnableRT = ConfigInterface->RenderingSetting().bEnableRayTracing && GraphicInterface->IsRayTracingEnabled();

	bEnableHWOcclusionRT = ConfigInterface->RenderingSetting().bEnableHardwareOcclusion;
	OcclusionThresholdRT = ConfigInterface->RenderingSetting().OcclusionTriangleThreshold;
	bEnableDepthPrepassRT = GraphicInterface->IsDepthPrePassEnabled();

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
		UHRenderTexture* Buffers[] = { nullptr
			, GSceneDiffuse
			, GSceneNormal
			, GSceneMaterial
			, GSceneDepth
			, GMotionVectorRT
			, GSceneMip
			, GRTShadowResult
			, GRTReflectionResult };

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

int32_t UHDeferredShadingRenderer::GetOccludedCallCount() const
{
	return OccludedCalls;
}

#endif

void UHDeferredShadingRenderer::UploadDataBuffers()
{
	UHGameTimerScope Scope("UploadDataBuffers", false);

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
	SystemConstantsCPU.GViewProj = CurrentCamera->GetViewProjMatrix();
	SystemConstantsCPU.GViewProjInv = CurrentCamera->GetInvViewProjMatrix();
	SystemConstantsCPU.GPrevViewProj_NonJittered = CurrentCamera->GetPrevViewProjMatrixNonJittered();
	SystemConstantsCPU.GViewProj_NonJittered = CurrentCamera->GetViewProjMatrixNonJittered();
	SystemConstantsCPU.GViewProjInv_NonJittered = CurrentCamera->GetInvViewProjMatrixNonJittered();
	SystemConstantsCPU.GView = CurrentCamera->GetViewMatrix();
	SystemConstantsCPU.GProjInv = CurrentCamera->GetInvProjMatrix();
	SystemConstantsCPU.GProjInv_NonJittered = CurrentCamera->GetInvProjMatrixNonJittered();

	SystemConstantsCPU.GResolution.x = static_cast<float>(RenderResolution.width);
	SystemConstantsCPU.GResolution.y = static_cast<float>(RenderResolution.height);
	SystemConstantsCPU.GResolution.z = 1.0f / SystemConstantsCPU.GResolution.x;
	SystemConstantsCPU.GResolution.w = 1.0f / SystemConstantsCPU.GResolution.y;

	SystemConstantsCPU.GCameraPos = CurrentCamera->GetPosition();
	SystemConstantsCPU.GCameraDir = CurrentCamera->GetForward();
	SystemConstantsCPU.GNumDirLights = static_cast<uint32_t>(CurrentScene->GetDirLightCount());
	SystemConstantsCPU.GNumPointLights = static_cast<uint32_t>(CurrentScene->GetPointLightCount());
	SystemConstantsCPU.GNumSpotLights = static_cast<uint32_t>(CurrentScene->GetSpotLightCount());
	SystemConstantsCPU.GMaxPointLightPerTile = MaxPointLightPerTile;
	SystemConstantsCPU.GMaxSpotLightPerTile = MaxSpotLightPerTile;

	uint32_t Dummy;
	GetLightCullingTileCount(SystemConstantsCPU.GLightTileCountX, Dummy);

	if (ConfigInterface->RenderingSetting().bTemporalAA)
	{
		XMFLOAT4 Offset = CurrentCamera->GetJitterOffset();
		SystemConstantsCPU.GJitterOffsetX = Offset.x;
		SystemConstantsCPU.GJitterOffsetY = Offset.y;
		SystemConstantsCPU.GJitterScaleMin = Offset.z;
		SystemConstantsCPU.GJitterScaleFactor = Offset.w;
	}

	// set sky light data
	UHSkyLightComponent* SkyLight = CurrentScene->GetSkyLight();
	SystemConstantsCPU.GAmbientSky = (SkyLight && SkyLight->IsEnabled()) ? SkyLight->GetSkyColor() * SkyLight->GetSkyIntensity() : XMFLOAT3();
	SystemConstantsCPU.GAmbientGround = (SkyLight && SkyLight->IsEnabled()) ? SkyLight->GetGroundColor() * SkyLight->GetGroundIntensity() : XMFLOAT3();

	SystemConstantsCPU.GShadowResolution.x = static_cast<float>(RTShadowExtent.width);
	SystemConstantsCPU.GShadowResolution.y = static_cast<float>(RTShadowExtent.height);
	SystemConstantsCPU.GShadowResolution.z = 1.0f / SystemConstantsCPU.GShadowResolution.x;
	SystemConstantsCPU.GShadowResolution.w = 1.0f / SystemConstantsCPU.GShadowResolution.y;

	UHTextureCube* SkyCube = GetCurrentSkyCube();

	SystemConstantsCPU.GNumRTInstances = RTInstanceCount;
	SystemConstantsCPU.GFrameNumber = GFrameNumber;

	// pack system rendering feature data
	uint32_t FeatureData = 0;
	FeatureData |= (GraphicInterface->IsDepthPrePassEnabled()) ? UH_ENUM_VALUE_U(UHSystemRenderFeatureBits::FeatureDepthPrePass) : 0;
	FeatureData |= (SkyCube != nullptr) ? UH_ENUM_VALUE_U(UHSystemRenderFeatureBits::FeatureEnvCube) : 0;
	FeatureData |= (GraphicInterface->IsHDRAvailable()) ? UH_ENUM_VALUE_U(UHSystemRenderFeatureBits::FeatureHDR) : 0;
	FeatureData |= (ConfigInterface->RenderingSetting().bEnableHardwareOcclusion) ? UH_ENUM_VALUE_U(UHSystemRenderFeatureBits::OcclusionCulling) : 0;
	SystemConstantsCPU.GSystemRenderFeature = FeatureData;

	SystemConstantsCPU.GDirectionalShadowRayTMax = ConfigInterface->RenderingSetting().RTShadowTMax;
	SystemConstantsCPU.GLinearClampSamplerIndex = LinearClampSamplerIndex;
	SystemConstantsCPU.GSkyCubeSamplerIndex = SkyCubeSamplerIndex;
	SystemConstantsCPU.GPointClampSamplerIndex = PointClampSamplerIndex;
	SystemConstantsCPU.RTReflectionQuality = ConfigInterface->RenderingSetting().RTReflectionQuality;
	SystemConstantsCPU.RTReflectionRayTMax = ConfigInterface->RenderingSetting().RTReflectionTMax;
	SystemConstantsCPU.RTReflectionSmoothCutoff = ConfigInterface->RenderingSetting().RTReflectionSmoothCutoff;
	SystemConstantsCPU.GFinalReflectionStrength = ConfigInterface->RenderingSetting().FinalReflectionStrength;
	SystemConstantsCPU.GEnvCubeMipMapCount = (SkyCube != nullptr) ? static_cast<float>(SkyCube->GetMipMapCount()) : 0;
	SystemConstantsCPU.GRefractionClearIndex = RefractionClearIndex;
	SystemConstantsCPU.GRefractionBlurIndex = RefractionBlurredIndex;
	SystemConstantsCPU.GDefaultAnisoSamplerIndex = DefaultSamplerIndex;

	GSystemConstantBuffer[CurrentFrameGT]->UploadAllData(&SystemConstantsCPU);

	// upload object constants, only update CPU value if transform is changed
	if (CurrentScene->GetAllRendererCount() > 0)
	{
		const std::vector<UHMeshRendererComponent*>& Renderers = CurrentScene->GetAllRenderers();
		int32_t MinDirtyObjIndex = static_cast<int32_t>(Renderers.size());
		int32_t MaxDirtyObjIndex = 0;

		for (size_t Idx = 0; Idx < Renderers.size(); Idx++)
		{
			// update CPU constants when the frame is dirty
			// the dirty flag is marked in UHMeshRendererComponent::Update()
			UHMeshRendererComponent* Renderer = Renderers[Idx];
			const int32_t RendererIdx = Renderer->GetBufferDataIndex();

			UHObjectConstants Constant = Renderer->GetConstants();
			if (Renderer->IsRenderDirty(CurrentFrameGT) && Renderer->IsVisible())
			{
				ObjectConstantsCPU[RendererIdx] = Constant;

				// setup occlusion data if necessary
				if (ConfigInterface->RenderingSetting().bEnableHardwareOcclusion)
				{
					Constant.GWorld = Renderer->GetWorldBoundMatrix();
					OcclusionConstantsCPU[RendererIdx] = Constant;
				}

				// record min/max dirty index to reduce buffer copy range
				MinDirtyObjIndex = std::min(MinDirtyObjIndex, RendererIdx);
				MaxDirtyObjIndex = std::max(MaxDirtyObjIndex, RendererIdx);

				Renderer->SetRenderDirty(false, CurrentFrameGT);
			}

			// copy material data only when it's dirty
			UHMaterial* Mat = Renderer->GetMaterial();
			if (Mat->IsRenderDirty(CurrentFrameGT))
			{
				Mat->UploadMaterialData(CurrentFrameGT);
				Mat->SetRenderDirty(false, CurrentFrameGT);
			}
		}

		if (MinDirtyObjIndex <= MaxDirtyObjIndex)
		{
			size_t CopySize = (MaxDirtyObjIndex - MinDirtyObjIndex + 1) * GObjectConstantBuffer[CurrentFrameGT]->GetBufferStride();
			GObjectConstantBuffer[CurrentFrameGT]->UploadData(&ObjectConstantsCPU[MinDirtyObjIndex], MinDirtyObjIndex, CopySize);

			if (ConfigInterface->RenderingSetting().bEnableHardwareOcclusion)
			{
				CopySize = (MaxDirtyObjIndex - MinDirtyObjIndex + 1) * GOcclusionConstantBuffer[CurrentFrameGT]->GetBufferStride();
				GOcclusionConstantBuffer[CurrentFrameGT]->UploadData(&OcclusionConstantsCPU[MinDirtyObjIndex], MinDirtyObjIndex, CopySize);
			}
		}
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
}

void UHDeferredShadingRenderer::FrustumCulling()
{
	UHGameTimerScope Scope("FrustumCulling", false);

	const UHCameraComponent* CurrentCamera = CurrentScene->GetMainCamera();
	if (!CurrentCamera)
	{
		return;
	}

	if (!CurrentCamera->IsEnabled())
	{
		return;
	}

	// async frustum culling task
	class UHFrustumCullingAsyncTask : public UHAsyncTask
	{
	public:
		void Init(UHScene* InScene, const int32_t InNumWorkerThreads)
		{
			CurrentScene = InScene;
			NumWorkerThreads = InNumWorkerThreads;
		}

		virtual void DoTask(const int32_t ThreadIdx) override
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

	private:
		UHScene* CurrentScene = nullptr;
		int32_t NumWorkerThreads = 0;
	};
	static UHFrustumCullingAsyncTask Tasks[GMaxWorkerThreads];

	// init and wake frustum culling task
	for (int32_t I = 0; I < NumWorkerThreads; I++)
	{
		Tasks[I].Init(CurrentScene, NumWorkerThreads);
		WorkerThreads[I]->ScheduleTask(&Tasks[I]);
		WorkerThreads[I]->WakeThread();
	}

	for (int32_t I = 0; I < NumWorkerThreads; I++)
	{
		WorkerThreads[I]->WaitTask();
	}
}

void UHDeferredShadingRenderer::CollectVisibleRenderer()
{
	UHGameTimerScope Scope("CollectVisibleRenderer", false);

	const UHCameraComponent* CurrentCamera = CurrentScene->GetMainCamera();
	if (!CurrentCamera)
	{
		return;
	}

	if (!CurrentCamera->IsEnabled())
	{
		return;
	}

	// recommend to reserve capacity during initialization
	OpaquesToRender.clear();
	MotionOpaquesToRender.clear();
	TranslucentsToRender.clear();
	OcclusionRenderers.clear();

	// reset counting counter
	for (int32_t Idx = 0; Idx < MaxCountingElement; Idx++)
	{
		CountingRenderers[Idx].clear();
	}

	const float CullingDistance = CurrentCamera->GetCullingDistance();
	for (UHMeshRendererComponent* Renderer : CurrentScene->GetAllRenderers())
	{
		if (Renderer->GetMaterial()->GetMaterialUsages().bIsSkybox || !Renderer->IsVisible())
		{
			continue;
		}

		// convert distance to counting index
		int32_t CountingIndex = static_cast<int32_t>(Renderer->GetSquareDistanceToMainCam() / (CullingDistance * CullingDistance) * MaxCountingElement);
		CountingIndex = std::min(CountingIndex, MaxCountingElement - 1);
		CountingRenderers[CountingIndex].push_back(Renderer);
	}

	// collect renderers from counting sort result, front-to-back for opaque
	for (int32_t CountIdx = 0; CountIdx < MaxCountingElement; CountIdx++)
	{
		for (size_t Idx = 0; Idx < CountingRenderers[CountIdx].size(); Idx++)
		{
			UHMeshRendererComponent* Renderer = CountingRenderers[CountIdx][Idx];
			const UHMaterial* Mat = CountingRenderers[CountIdx][Idx]->GetMaterial();
			const UHMesh* Mesh = Renderer->GetMesh();
			const int32_t TriCount = Mesh->GetIndicesCount() / 3;
			const bool bOcclusionTest = bEnableHWOcclusionRT && TriCount >= OcclusionThresholdRT;

			if (Mat->IsOpaque())
			{
				OpaquesToRender.push_back(Renderer);
				if (Renderer->IsMotionDirty(CurrentFrameGT) && !GraphicInterface->IsMeshShaderSupported())
				{
					MotionOpaquesToRender.push_back(Renderer);
					Renderer->SetMotionDirty(false, CurrentFrameGT);
				}
			}

			if (bOcclusionTest)
			{
				OcclusionRenderers.push_back(Renderer);
			}
		}
	}

	// back-to-front for translucent
	for (int32_t CountIdx = MaxCountingElement - 1; CountIdx >= 0; CountIdx--)
	{
		for (int32_t Idx = (int32_t)CountingRenderers[CountIdx].size() - 1; Idx >= 0; Idx--)
		{
			UHMeshRendererComponent* Renderer = CountingRenderers[CountIdx][Idx];
			const UHMaterial* Mat = CountingRenderers[CountIdx][Idx]->GetMaterial();
			if (!Mat->IsOpaque())
			{
				TranslucentsToRender.push_back(Renderer);
				if (Mat->GetMaterialUsages().bUseRefraction)
				{
					bHasRefractionMaterialGT = true;
				}
			}
		}
	}
}

void UHDeferredShadingRenderer::CollectMeshShaderInstance()
{
	UHGameTimerScope Scope("CollectMeshShaderInstance", false);

	const UHCameraComponent* CurrentCamera = CurrentScene->GetMainCamera();
	if (!CurrentCamera || !GraphicInterface->IsMeshShaderSupported())
	{
		return;
	}

	if (!CurrentCamera->IsEnabled())
	{
		return;
	}

	// reset all counters & clear sorted visiable mesh shader group index
	SortedMeshShaderGroupIndex.clear();
	for (size_t Idx = 0; Idx < CurrentScene->GetMaterialCount(); Idx++)
	{
		MeshShaderInstancesCounter[Idx] = 0;
		VisibleMeshShaderData[Idx].clear();
		MotionOpaqueMeshShaderData[Idx].clear();
		MotionTranslucentMeshShaderData[Idx].clear();
	}

	// collect visible mesh shader instances for opaque objects
	for (UHMeshRendererComponent* Renderer : OpaquesToRender)
	{
		const UHMesh* Mesh = Renderer->GetMesh();
		const bool bOcclusionTest = bEnableHWOcclusionRT && ((int32_t)Mesh->GetIndicesCount() / 3) >= OcclusionThresholdRT;

		// set the instance to the corresponding material and it's current rendering index
		const uint32_t MatDataIndex = Renderer->GetMaterial()->GetBufferDataIndex();
		const int32_t NewIndex = MeshShaderInstancesCounter[MatDataIndex]++;

		// system will dispatch opaque renderer front-to-back for each material group
		// but the group isn't sorted, so add the material data index to the list when first instance is occurred.
		if (NewIndex == 0)
		{
			SortedMeshShaderGroupIndex.push_back(MatDataIndex);
		}

		UHMeshShaderData Data;
		Data.RendererIndex = Renderer->GetBufferDataIndex();
		Data.bDoOcclusionTest = bOcclusionTest ? 1 : 0;
		bool bClearMotionDirty = false;

		for (uint32_t MeshletIdx = 0; MeshletIdx < Mesh->GetMeshletCount(); MeshletIdx++)
		{
			Data.MeshletIndex = MeshletIdx;
			VisibleMeshShaderData[MatDataIndex].push_back(Data);

			// push to motion mesh shader data list if it's motion dirty
			if (Renderer->IsMotionDirty(CurrentFrameGT))
			{
				MotionOpaqueMeshShaderData[MatDataIndex].push_back(Data);
				bClearMotionDirty = true;
			}
		}

		if (bClearMotionDirty)
		{
			Renderer->SetMotionDirty(false, CurrentFrameGT);
		}
	}

	// collect visible mesh shader instances for translucent objects, only for motion
	for (int32_t Idx = (int32_t)TranslucentsToRender.size() - 1; Idx >= 0; Idx--)
	{
		UHMeshRendererComponent* Renderer = TranslucentsToRender[Idx];
		const UHMesh* Mesh = Renderer->GetMesh();
		const bool bOcclusionTest = bEnableHWOcclusionRT && ((int32_t)Mesh->GetIndicesCount() / 3) >= OcclusionThresholdRT;

		const uint32_t MatDataIndex = Renderer->GetMaterial()->GetBufferDataIndex();
		const int32_t NewIndex = MeshShaderInstancesCounter[MatDataIndex]++;

		if (NewIndex == 0)
		{
			SortedMeshShaderGroupIndex.push_back(MatDataIndex);
		}

		UHMeshShaderData Data;
		Data.RendererIndex = Renderer->GetBufferDataIndex();
		Data.bDoOcclusionTest = bOcclusionTest ? 1 : 0;

		// translucent always output motion for now
		for (uint32_t MeshletIdx = 0; MeshletIdx < Mesh->GetMeshletCount(); MeshletIdx++)
		{
			Data.MeshletIndex = MeshletIdx;
			MotionTranslucentMeshShaderData[MatDataIndex].push_back(Data);
		}
	}

	// mesh shader group size shouldn't be bigger than total material count
	assert(SortedMeshShaderGroupIndex.size() <= CurrentScene->GetMaterialCount());

	for (size_t Idx = 0; Idx < CurrentScene->GetMaterialCount(); Idx++)
	{
		if (VisibleMeshShaderData[Idx].size() > 0)
		{
			GMeshShaderData[CurrentFrameGT][Idx]->UploadData(VisibleMeshShaderData[Idx].data(), 0, VisibleMeshShaderData[Idx].size() * sizeof(UHMeshShaderData));
		}

		if (MotionOpaqueMeshShaderData[Idx].size() > 0)
		{
			GMotionOpaqueShaderData[CurrentFrameGT][Idx]->UploadData(MotionOpaqueMeshShaderData[Idx].data(), 0, MotionOpaqueMeshShaderData[Idx].size() * sizeof(UHMeshShaderData));
		}

		if (MotionTranslucentMeshShaderData[Idx].size() > 0)
		{
			GMotionTranslucentShaderData[CurrentFrameGT][Idx]->UploadData(MotionTranslucentMeshShaderData[Idx].data(), 0
				, MotionTranslucentMeshShaderData[Idx].size() * sizeof(UHMeshShaderData));
		}
	}
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
				OcclusionParallelSubmitter.CollectCurrentFrameRTBundle(CurrentFrameRT);
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
				// first-chance resource barriers and resets
				if (!bIsPresentedPreviously || bIsSwapChainResetRT)
				{
					SceneRenderBuilder.PushResourceBarrier(UHImageBarrier(GOpaqueSceneResult, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
					SceneRenderBuilder.PushResourceBarrier(UHImageBarrier(GQuarterBlurredScene, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
					SceneRenderBuilder.FlushResourceBarrier();
				}

				RenderDepthPrePass(SceneRenderBuilder);
				RenderBasePass(SceneRenderBuilder);
				RenderOcclusionPass(SceneRenderBuilder);
				RenderMotionPass(SceneRenderBuilder);

				if (!bEnableAsyncComputeRT)
				{
					BuildTopLevelAS(SceneRenderBuilder);
					GenerateSH9Pass(SceneRenderBuilder);
				}

				DispatchLightCulling(SceneRenderBuilder);
				DispatchRayShadowPass(SceneRenderBuilder);

				RenderLightPass(SceneRenderBuilder);
				RenderSkyPass(SceneRenderBuilder);

				PreReflectionPass(SceneRenderBuilder);
				DispatchRayReflectionPass(SceneRenderBuilder);
				DrawReflectionPass(SceneRenderBuilder);

				RenderTranslucentPass(SceneRenderBuilder);
				RenderPostProcessing(SceneRenderBuilder);
			}

			// blit scene to swap chain
			PresentIndex = RenderSceneToSwapChain(SceneRenderBuilder);

		#if WITH_EDITOR
			// get GPU times
			for (int32_t Idx = 0; Idx < UH_ENUM_VALUE(UHRenderPassTypes::UHRenderPassMax); Idx++)
			{
				GPUTimeQueries[Idx]->ResolveTimeStamp(SceneRenderBuilder.GetCmdList());
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
		OccludedCalls = SceneRenderBuilder.OccludedCalls;
	#endif

		// wait until the previous presentation is done, to prevent glitches on some hardwares
		if (bIsPresentedPreviously && !bIsSwapChainResetRT)
		{
			SceneRenderBuilder.WaitFence(SceneRenderQueue.Fences[(CurrentFrameRT - 1) % GMaxFrameInFlight]);
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

		WorkerThreads[ThreadIdx]->DoTask(ThreadIdx);
		WorkerThreads[ThreadIdx]->NotifyTaskDone();
	}
}