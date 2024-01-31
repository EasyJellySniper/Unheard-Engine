#include "DeferredShadingRenderer.h"
#include "DescriptorHelper.h"

// all init/create/release implementations of DeferredShadingRenderer are put here
#if WITH_EDITOR
UHDeferredShadingRenderer* UHDeferredShadingRenderer::SceneRendererEditorOnly = nullptr;
#endif

UHDeferredShadingRenderer::UHDeferredShadingRenderer(UHGraphic* InGraphic, UHAssetManager* InAssetManager, UHConfigManager* InConfig, UHGameTimer* InTimer)
	: GraphicInterface(InGraphic)
	, AssetManagerInterface(InAssetManager)
	, ConfigInterface(InConfig)
	, TimerInterface(InTimer)
	, RenderResolution(VkExtent2D())
	, RTShadowExtent(VkExtent2D())
	, CurrentFrameGT(0)
	, CurrentFrameRT(0)
	, FrameNumberRT(0)
	, bIsResetNeededShared(false)
	, bEnableAsyncComputeGT(false)
	, bEnableAsyncComputeRT(false)
	, CurrentScene(nullptr)
	, SystemConstantsCPU(UHSystemConstants())
	, SkyMeshRT(nullptr)
	, DefaultSamplerIndex(-1)
	, bEnableDepthPrePass(false)
	, PostProcessResultIdx(0)
	, bIsTemporalReset(true)
	, RTInstanceCount(0)
	, IndicesTypeBuffer(nullptr)
	, NumWorkerThreads(0)
	, RenderThread(nullptr)
	, bVsyncRT(false)
	, bIsSwapChainResetGT(false)
	, bIsSwapChainResetRT(false)
	, bIsRenderingEnabledRT(true)
	, bIsSkyLightEnabledRT(true)
	, bNeedGenerateSH9RT(true)
	, ParallelTask(UHParallelTask::None)
	, LightCullingTileSize(16)
	, MaxPointLightPerTile(32)
	, MaxSpotLightPerTile(32)
#if WITH_EDITOR
	, DebugViewIndex(0)
	, RenderThreadTime(0)
	, DrawCalls(0)
	, EditorWidthDelta(0)
	, EditorHeightDelta(0)
	, bDrawDebugViewRT(true)
#endif
	, bHasRefractionMaterialGT(false)
	, bHasRefractionMaterialRT(false)
	, FrontmostRefractionIndexGT(UHINDEXNONE)
	, FrontmostRefractionIndexRT(UHINDEXNONE)
{
	for (int32_t Idx = 0; Idx < NumOfPostProcessRT; Idx++)
	{
		PostProcessResults[Idx] = nullptr;
	}

	for (int32_t Idx = 0; Idx < UHRenderPassMax; Idx++)
	{
		GPUTimeQueries[Idx] = nullptr;
#if WITH_EDITOR
		GPUTimes[Idx] = 0.0f;
#endif
	}

#if WITH_EDITOR
	SceneRendererEditorOnly = this;
#endif
}

bool UHDeferredShadingRenderer::Initialize(UHScene* InScene)
{
	// scene setup
	CurrentScene = InScene;
	PrepareMeshes();
	PrepareTextures();
	PrepareSamplers();
	GSkyLightCube = GetCurrentSkyCube();

	// config setup
	bEnableDepthPrePass = GraphicInterface->IsDepthPrePassEnabled();
	NumWorkerThreads = ConfigInterface->RenderingSetting().ParallelThreads;
	bEnableAsyncComputeGT = ConfigInterface->RenderingSetting().bEnableAsyncCompute;

	const bool bIsRendererSuccess = InitQueueSubmitters();
	if (bIsRendererSuccess)
	{
		// create render pass stuffs
		CreateRenderingBuffers();
		CreateRenderPasses();
		CreateRenderFrameBuffers();
		PrepareRenderingShaders();

		// create data buffers
		CreateDataBuffers();

		// update descriptor binding
		UpdateDescriptors();

		// create thread objects
		CreateThreadObjects();

		// reserve enough space for renderers, save the allocation time
		OpaquesToRender.reserve(CurrentScene->GetOpaqueRenderers().size() * 2);
		TranslucentsToRender.reserve(CurrentScene->GetTranslucentRenderers().size() * 2);
	}

	return bIsRendererSuccess;
}

void UHDeferredShadingRenderer::Release()
{
	VkDevice LogicalDevice = GraphicInterface->GetLogicalDevice();

	// wait device to finish before release
	GraphicInterface->WaitGPU();

	// end render thread
	RenderThread->WaitTask();
	RenderThread->EndThread();
	for (auto& WorkerThread : WorkerThreads)
	{
		WorkerThread->EndThread();
	}
	WorkerThreads.clear();

	RelaseRenderingBuffers();
	ReleaseDataBuffers();
	ReleaseRenderPassObjects();
	ReleaseShaders();

	if (bEnableAsyncComputeGT)
	{
		AsyncComputeQueue.Release();
	}
	SceneRenderQueue.Release();
	DepthParallelSubmitter.Release();
	BaseParallelSubmitter.Release();
	MotionOpaqueParallelSubmitter.Release();
	MotionTranslucentParallelSubmitter.Release();
	TranslucentParallelSubmitter.Release();
}

void UHDeferredShadingRenderer::PrepareMeshes()
{
	// create mesh buffer for all default lit renderers
	const std::vector<UHMeshRendererComponent*>& Renderers = CurrentScene->GetAllRenderers();

	// needs the cmd buffer
	VkCommandBuffer CreationCmd = GraphicInterface->BeginOneTimeCmd();

	UHMesh* SkyMesh = AssetManagerInterface->GetMesh("UHMesh_Cube");
	if (SkyMesh)
	{
		SkyMesh->CreateGPUBuffers(GraphicInterface);
	}

	for (const UHMeshRendererComponent* Renderer : Renderers)
	{
		UHMesh* Mesh = Renderer->GetMesh();
		UHMaterial* Mat = Renderer->GetMaterial();
		Mesh->CreateGPUBuffers(GraphicInterface);

		if (!Mat->GetMaterialUsages().bIsSkybox)
		{
			Mesh->CreateBottomLevelAS(GraphicInterface, CreationCmd);
		}
	}
	GraphicInterface->EndOneTimeCmd(CreationCmd);

	// create top level AS after bottom level AS is done
	// can't be created in the same command line!! All bottom level AS must be created before creating top level AS
	CreationCmd = GraphicInterface->BeginOneTimeCmd();
	for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		UH_SAFE_RELEASE(TopLevelAS[Idx]);
		TopLevelAS[Idx] = GraphicInterface->RequestAccelerationStructure();
		RTInstanceCount = TopLevelAS[Idx]->CreateTopAS(Renderers, CreationCmd);
	}
	GraphicInterface->EndOneTimeCmd(CreationCmd);

	// release scratch data of AS after creation
	for (const UHMeshRendererComponent* Renderer : Renderers)
	{
		if (Renderer->GetMesh()->GetBottomLevelAS())
		{
			Renderer->GetMesh()->GetBottomLevelAS()->ReleaseScratch();
		}
	}
}

void UHDeferredShadingRenderer::CheckTextureReference(UHMaterial* InMat)
{
	VkCommandBuffer CreationCmd = GraphicInterface->BeginOneTimeCmd();
	UHRenderBuilder RenderBuilder(GraphicInterface, CreationCmd);

	for (const std::string RegisteredTexture : InMat->GetRegisteredTextureNames())
	{
		UHTexture2D* Tex = AssetManagerInterface->GetTexture2D(RegisteredTexture);
		if (Tex)
		{
			Tex->UploadToGPU(GraphicInterface, RenderBuilder);
		}
	}

	AssetManagerInterface->MapTextureIndex(InMat);
	GraphicInterface->EndOneTimeCmd(CreationCmd);
}

void UHDeferredShadingRenderer::PrepareTextures()
{
	// instead of uploading to GPU right after import, I choose to upload textures if they're really in use
	// this makes workflow complicated but good for GPU memory

	// Step1: uploading all textures which are really using for rendering
	for (const UHMeshRendererComponent* Renderer : CurrentScene->GetAllRenderers())
	{
		UHMaterial* Mat = Renderer->GetMaterial();
		CheckTextureReference(Mat);
	}
	
	VkCommandBuffer CreationCmd = GraphicInterface->BeginOneTimeCmd();
	UHRenderBuilder RenderBuilder(GraphicInterface, CreationCmd);

	// Step2: Build all cubemaps in use
	if (UHTextureCube* Cube = GetCurrentSkyCube())
	{
		Cube->Build(GraphicInterface, RenderBuilder);
	}

	GraphicInterface->EndOneTimeCmd(CreationCmd);
}

void UHDeferredShadingRenderer::PrepareSamplers()
{
	// shared sampler requests
	UHSamplerInfo PointClampedInfo(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	PointClampedInfo.MaxAnisotropy = 1;
	GPointClampedSampler = GraphicInterface->RequestTextureSampler(PointClampedInfo);

#if WITH_EDITOR
	// request a sampler for preview scene
	PointClampedInfo.MipBias = 0;
	GraphicInterface->RequestTextureSampler(PointClampedInfo);
#endif

	UHSamplerInfo LinearClampedInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	LinearClampedInfo.MaxAnisotropy = 1;
	GLinearClampedSampler = GraphicInterface->RequestTextureSampler(LinearClampedInfo);

	LinearClampedInfo.MaxAnisotropy = 16;
	GAnisoClampedSampler = GraphicInterface->RequestTextureSampler(LinearClampedInfo);
	DefaultSamplerIndex = UHUtilities::FindIndex(GraphicInterface->GetSamplers(), GAnisoClampedSampler);

	GSkyCubeSampler = GraphicInterface->RequestTextureSampler(UHSamplerInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT
		, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT));
}

void UHDeferredShadingRenderer::PrepareRenderingShaders()
{
	// bindless table
	TextureTable = MakeUnique<UHTextureTable>(GraphicInterface, "TextureTable");
	SamplerTable = MakeUnique<UHSamplerTable>(GraphicInterface, "SamplerTable");
	const std::vector<VkDescriptorSetLayout> BindlessLayouts = { TextureTable->GetDescriptorSetLayout(), SamplerTable->GetDescriptorSetLayout()};

	// this loop will create all material shaders
	for (UHMeshRendererComponent* Renderer : CurrentScene->GetAllRenderers())
	{
		UHMaterial* Mat = Renderer->GetMaterial();
		if (Mat && !Mat->GetMaterialUsages().bIsSkybox)
		{
			RecreateMaterialShaders(Renderer, Mat);
		}
	}

	// down sample depth shader
	DownsampleDepthShader = MakeUnique<UHDownsampleDepthShader>(GraphicInterface, "DownsampleDepthShader");

	// light culling and pass shaders
	LightCullingShader = MakeUnique<UHLightCullingShader>(GraphicInterface, "LightCullingShader");
	LightPassShader = MakeUnique<UHLightPassShader>(GraphicInterface, "LightPassShader", RTInstanceCount);

	// sky pass shader
	SkyPassShader = MakeUnique<UHSkyPassShader>(GraphicInterface, "SkyPassShader", SkyboxPassObj.RenderPass);
	SH9Shader = MakeUnique<UHSphericalHarmonicShader>(GraphicInterface, "SH9Shader");

	// motion pass shader
	MotionCameraShader = MakeUnique<UHMotionCameraPassShader>(GraphicInterface, "MotionCameraPassShader", MotionCameraPassObj.RenderPass);
	if (GraphicInterface->IsAMDIntegratedGPU())
	{
		MotionCameraWorkaroundShader = MakeUnique<UHMotionCameraPassShader>(GraphicInterface, "MotionCameraWorkaroundShader", MotionOpaquePassObj.RenderPass);
	}

	// post processing shaders
	TemporalAAShader = MakeUnique<UHTemporalAAShader>(GraphicInterface, "TemporalAAShader");
	ToneMapShader = MakeUnique<UHToneMappingShader>(GraphicInterface, "ToneMapShader", PostProcessPassObj[0].RenderPass);

	OpaqueBlurHShader = MakeUnique<UHGaussianFilterShader>(GraphicInterface, "FilterHShader", UHGaussianFilterType::FilterHorizontal);
	OpaqueBlurVShader = MakeUnique<UHGaussianFilterShader>(GraphicInterface, "FilterVShader", UHGaussianFilterType::FilterVertical);
	TranslucentBlurHShader = MakeUnique<UHGaussianFilterShader>(GraphicInterface, "FilterHShader", UHGaussianFilterType::FilterHorizontal);
	TranslucentBlurVShader = MakeUnique<UHGaussianFilterShader>(GraphicInterface, "FilterVShader", UHGaussianFilterType::FilterVertical);

	// RT shaders
	if (GraphicInterface->IsRayTracingEnabled() && RTInstanceCount > 0)
	{
		RecreateRTShaders(nullptr, true);
		SoftRTShadowShader = MakeUnique<UHSoftRTShadowShader>(GraphicInterface, "SoftRTShadowShader");
	}

#if WITH_EDITOR
	DebugViewShader = MakeUnique<UHDebugViewShader>(GraphicInterface, "DebugViewShader", PostProcessPassObj[0].RenderPass);
	DebugBoundShader = MakeUnique<UHDebugBoundShader>(GraphicInterface, "DebugBoundShader", PostProcessPassObj[0].RenderPass);
#endif
}

bool UHDeferredShadingRenderer::InitQueueSubmitters()
{
	VkDevice LogicalDevice = GraphicInterface->GetLogicalDevice();
	const UHQueueFamily& QueueFamily = GraphicInterface->GetQueueFamily();

	bool bAsyncInitSucceed = true;
	if (bEnableAsyncComputeGT)
	{
		bAsyncInitSucceed &= AsyncComputeQueue.Initialize(LogicalDevice, QueueFamily.ComputesFamily.value(), 0);
	}

	return bAsyncInitSucceed && SceneRenderQueue.Initialize(LogicalDevice, QueueFamily.GraphicsFamily.value(), 0);
}

void UHDeferredShadingRenderer::UpdateDescriptors()
{
	VkDevice LogicalDevice = GraphicInterface->GetLogicalDevice();
	GSkyLightCube = GetCurrentSkyCube();

	// ------------------------------------------------ Bindless table update
	UpdateTextureDescriptors();

	// ------------------------------------------------ Depth pass descriptor update
	if (bEnableDepthPrePass)
	{
		for (const UHMeshRendererComponent* Renderer : CurrentScene->GetOpaqueRenderers())
		{
			assert(DepthPassShaders.find(Renderer->GetBufferDataIndex()) != DepthPassShaders.end());
			DepthPassShaders[Renderer->GetBufferDataIndex()]->BindParameters(Renderer);
		}
	}
	DownsampleDepthShader->BindParameters();

	// ------------------------------------------------ Base pass descriptor update
	for (const UHMeshRendererComponent* Renderer : CurrentScene->GetOpaqueRenderers())
	{
		assert(BasePassShaders.find(Renderer->GetBufferDataIndex()) != BasePassShaders.end());
		BasePassShaders[Renderer->GetBufferDataIndex()]->BindParameters(Renderer);
	}

	// ------------------------------------------------ Lighting culling descriptor update
	LightCullingShader->BindParameters();

	// ------------------------------------------------ Lighting pass descriptor update
	LightPassShader->BindParameters();

	// ------------------------------------------------ sky pass descriptor update
	SH9Shader->BindParameters();
	SkyPassShader->BindParameters();

	// ------------------------------------------------ motion pass descriptor update
	MotionCameraShader->BindParameters();
	if (MotionCameraWorkaroundShader)
	{
		MotionCameraWorkaroundShader->BindParameters();
	}

	for (const UHMeshRendererComponent* Renderer : CurrentScene->GetAllRenderers())
	{
		UHMaterial* Mat = Renderer->GetMaterial();
		if (Mat == nullptr || Mat->GetMaterialUsages().bIsSkybox)
		{
			continue;
		}

		const bool bIsOpaque = Mat->GetBlendMode() <= UHBlendMode::Masked;
		const int32_t RendererIdx = Renderer->GetBufferDataIndex();
		if (bIsOpaque)
		{
			assert(MotionOpaqueShaders.find(RendererIdx) != MotionOpaqueShaders.end());
			MotionOpaqueShaders[RendererIdx]->BindParameters(Renderer);
		}
		else
		{
			assert(MotionTranslucentShaders.find(RendererIdx) != MotionTranslucentShaders.end());
			MotionTranslucentShaders[RendererIdx]->BindParameters(Renderer);
		}
	}

	// ------------------------------------------------ translucent pass descriptor update
	for (const UHMeshRendererComponent* Renderer : CurrentScene->GetTranslucentRenderers())
	{
		assert(TranslucentPassShaders.find(Renderer->GetBufferDataIndex()) != TranslucentPassShaders.end());
		TranslucentPassShaders[Renderer->GetBufferDataIndex()]->BindParameters(Renderer);
	}

	// ------------------------------------------------ post process pass descriptor update
	// when binding post processing input, be sure to bind it alternately, the two RT will keep bliting to each other
	// the post process RT binding will be in PostProcessRendering.cpp
	ToneMapShader->BindParameters();
	TemporalAAShader->BindParameters();

	OpaqueBlurHShader->BindParameters();
	OpaqueBlurVShader->BindParameters();
	TranslucentBlurHShader->BindParameters();
	TranslucentBlurVShader->BindParameters();

	// ------------------------------------------------ ray tracing pass descriptor update
	if (GraphicInterface->IsRayTracingEnabled() && RTInstanceCount > 0)
	{
		// bind VB/IB table
		const std::vector<UHMeshRendererComponent*>& Renderers = CurrentScene->GetAllRenderers();
		std::vector<UHRenderBuffer<XMFLOAT2>*> UVs;
		std::vector<VkDescriptorBufferInfo> BufferInfos;
		std::vector<int32_t> IndexTypes;

		// don't need to create another buffer for VB/IB since they're already there
		// simply setup VkDescriptorBufferInfo for them
		for (int32_t Idx = 0; Idx < static_cast<int32_t>(Renderers.size()); Idx++)
		{
			const UHMaterial* Mat = Renderers[Idx]->GetMaterial();
			const UHMesh* Mesh = Renderers[Idx]->GetMesh();
			if (!Mat || Mat->GetMaterialUsages().bIsSkybox || !Mesh)
			{
				continue;
			}

			UVs.push_back(Mesh->GetUV0Buffer());

			// collect index buffer info based on index type
			VkDescriptorBufferInfo NewInfo{};
			if (Mesh->IsIndexBufer32Bit())
			{
				NewInfo.buffer = Mesh->GetIndexBuffer()->GetBuffer();
				NewInfo.range = Mesh->GetIndexBuffer()->GetBufferSize();
				IndexTypes.push_back(1);
			}
			else
			{
				NewInfo.buffer = Mesh->GetIndexBuffer16()->GetBuffer();
				NewInfo.range = Mesh->GetIndexBuffer16()->GetBufferSize();
				IndexTypes.push_back(0);
			}
			BufferInfos.push_back(NewInfo);
		}
		RTVertexTable->BindStorage(UVs, 0);
		RTIndicesTable->BindStorage(BufferInfos, 0);

		// upload index types
		IndicesTypeBuffer->UploadAllData(IndexTypes.data());
		RTIndicesTypeTable->BindStorage(IndicesTypeBuffer.get(), 0, 0, true);

		// bind RT material data
		for (int32_t FrameIdx = 0; FrameIdx < GMaxFrameInFlight; FrameIdx++)
		{
			std::vector<UHRenderBuffer<UHRTMaterialData>*> RTMaterialData;
			const std::vector<UHMaterial*>& AllMaterials = CurrentScene->GetMaterials();
			for (size_t Idx = 0; Idx < AllMaterials.size(); Idx++)
			{
				RTMaterialData.push_back(AllMaterials[Idx]->GetRTMaterialDataGPU(FrameIdx));
			}
			RTMaterialDataTable->BindStorage(RTMaterialData, 0);
		}

		RTShadowShader->BindParameters();
		SoftRTShadowShader->BindParameters();

		if (ConfigInterface->RenderingSetting().RTDirectionalShadowQuality == 0)
		{
			SoftRTShadowShader->BindImage(GSceneDepth, 3);
			SoftRTShadowShader->BindImage(GSceneTranslucentDepth, 4);
		}
		else
		{
			SoftRTShadowShader->BindImage(GHalfDepth, 3);
			SoftRTShadowShader->BindImage(GHalfTranslucentDepth, 4);
		}
	}

	// ------------------------------------------------ debug passes descriptor update
#if WITH_EDITOR
	DebugViewShader->BindParameters();
	DebugBoundShader->BindParameters();
#endif
}

void UHDeferredShadingRenderer::ReleaseShaders()
{
	if (bEnableDepthPrePass)
	{
		for (auto& Shader : DepthPassShaders)
		{
			Shader.second->Release();
		}
		DepthPassShaders.clear();
	}
	DownsampleDepthShader->Release();

	for (auto& Shader : BasePassShaders)
	{
		Shader.second->Release();
	}
	BasePassShaders.clear();

	for (auto& Shader : MotionOpaqueShaders)
	{
		Shader.second->Release();
	}
	MotionOpaqueShaders.clear();

	for (auto& Shader : MotionTranslucentShaders)
	{
		Shader.second->Release();
	}
	MotionTranslucentShaders.clear();

	for (auto& Shader : TranslucentPassShaders)
	{
		Shader.second->Release();
	}
	TranslucentPassShaders.clear();

	LightCullingShader->Release();
	LightPassShader->Release();
	SkyPassShader->Release();
	SH9Shader->Release();
	MotionCameraShader->Release();
	UH_SAFE_RELEASE(MotionCameraWorkaroundShader);
	TemporalAAShader->Release();
	ToneMapShader->Release();
	OpaqueBlurHShader->Release();
	OpaqueBlurVShader->Release();
	TranslucentBlurHShader->Release();
	TranslucentBlurVShader->Release();

	if (GraphicInterface->IsRayTracingEnabled())
	{
		UH_SAFE_RELEASE(RTDefaultHitGroupShader);
		UH_SAFE_RELEASE(RTShadowShader);
		UH_SAFE_RELEASE(RTVertexTable);
		UH_SAFE_RELEASE(RTIndicesTable);
		UH_SAFE_RELEASE(RTIndicesTypeTable);
		UH_SAFE_RELEASE(RTMaterialDataTable);
		UH_SAFE_RELEASE(SoftRTShadowShader);
	}

	TextureTable->Release();
	SamplerTable->Release();

#if WITH_EDITOR
	DebugViewShader->Release();
	DebugBoundShader->Release();
#endif
}

void UHDeferredShadingRenderer::CreateRenderingBuffers()
{
	// init formats
	const UHTextureFormat DiffuseFormat = UH_FORMAT_RGBA8_SRGB;
	const UHTextureFormat NormalFormat = UH_FORMAT_A2R10G10B10;
	const UHTextureFormat SpecularFormat = UH_FORMAT_RGBA8_UNORM;
	const UHTextureFormat SceneResultFormat = UH_FORMAT_RGBA16F;
	const UHTextureFormat HistoryResultFormat = UH_FORMAT_R11G11B10;
	const UHTextureFormat SceneMipFormat = UH_FORMAT_R16_UNORM;
	const UHTextureFormat DepthFormat = (GraphicInterface->Is24BitDepthSupported()) ? UH_FORMAT_X8_D24 : UH_FORMAT_D32F;
	const UHTextureFormat HDRFormat = UH_FORMAT_RGBA16F;
	const UHTextureFormat MotionFormat = UH_FORMAT_RG16F;
	const UHTextureFormat HalfDepthFormat = UH_FORMAT_R32F;

	// create GBuffer
	RenderResolution.width = ConfigInterface->RenderingSetting().RenderWidth;
	RenderResolution.height = ConfigInterface->RenderingSetting().RenderHeight;

	// GBuffers, see shader usage for each of them
	GSceneDiffuse = GraphicInterface->RequestRenderTexture("GBufferA", RenderResolution, DiffuseFormat);
	GSceneNormal = GraphicInterface->RequestRenderTexture("GBufferB", RenderResolution, NormalFormat);
	GSceneMaterial = GraphicInterface->RequestRenderTexture("GBufferC", RenderResolution, SpecularFormat);
	GSceneResult = GraphicInterface->RequestRenderTexture("SceneResult", RenderResolution, SceneResultFormat, true);
	GSceneMip = GraphicInterface->RequestRenderTexture("SceneMip", RenderResolution, SceneMipFormat);

	// depth buffer, also needs another depth buffer with translucent
	GSceneDepth = GraphicInterface->RequestRenderTexture("SceneDepth", RenderResolution, DepthFormat);
	GSceneTranslucentDepth = GraphicInterface->RequestRenderTexture("SceneTranslucentDepth", RenderResolution, DepthFormat);

	// half res depth buffer
	VkExtent2D HalfDepthResolution;
	HalfDepthResolution.width = RenderResolution.width >> 1;
	HalfDepthResolution.height = RenderResolution.height >> 1;
	GHalfDepth = GraphicInterface->RequestRenderTexture("HalfDepth", HalfDepthResolution, HalfDepthFormat, true);
	GHalfTranslucentDepth = GraphicInterface->RequestRenderTexture("HalfTranslucentDepth", HalfDepthResolution, HalfDepthFormat, true);

	// vertex normal buffer for saving the "search ray" in RT shadows
	GSceneVertexNormal = GraphicInterface->RequestRenderTexture("SceneVertexNormal", RenderResolution, NormalFormat);
	GSceneTranslucentVertexNormal = GraphicInterface->RequestRenderTexture("SceneTranslucentVertexNormal", RenderResolution, NormalFormat);

	// post process buffer, use the same format as scene result
	GPostProcessRT = GraphicInterface->RequestRenderTexture("PostProcessRT", RenderResolution, SceneResultFormat, true);
	GPreviousSceneResult = GraphicInterface->RequestRenderTexture("PreviousResultRT", RenderResolution, HistoryResultFormat);
	GOpaqueSceneResult = GraphicInterface->RequestRenderTexture("OpaqueSceneResult", RenderResolution, HistoryResultFormat);

	// quarter blurred RT, use R11G11B10 should suffice
	VkExtent2D QuarterBlurredResolution;
	QuarterBlurredResolution.width = RenderResolution.width >> 2;
	QuarterBlurredResolution.height = RenderResolution.height >> 2;
	GQuarterBlurredScene = GraphicInterface->RequestRenderTexture("QuarterBlurredScene", QuarterBlurredResolution, HistoryResultFormat);

	// gaussian temp RTs, use R11G11B10 should suffice
	// though this is created with render resolution, the rendering could only use partial size of it depend on down size factor
	GGaussianFilterTempRT0 = GraphicInterface->RequestRenderTexture("GaussianFilterTemp0", RenderResolution, HistoryResultFormat, true);
	GGaussianFilterTempRT1 = GraphicInterface->RequestRenderTexture("GaussianFilterTemp1", RenderResolution, HistoryResultFormat, true);

	// motion vector buffer
	GMotionVectorRT = GraphicInterface->RequestRenderTexture("MotionVectorRT", RenderResolution, MotionFormat);

	// rt shadows buffer
	ResizeRayTracingBuffers(true);

	// create light culling tile buffer
	uint32_t TileCountX, TileCountY;
	GetLightCullingTileCount(TileCountX, TileCountY);

	// data size per tile: (MaxPointLightPerTile + 1), the extra one is for the number of lights in this tile
	// so it's TileCountX * TileCountY * (MaxPointLightPerTile + 1) in total
	GPointLightListBuffer = GraphicInterface->RequestRenderBuffer<uint32_t>(TileCountX * TileCountY * (MaxPointLightPerTile + 1), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	GPointLightListTransBuffer = GraphicInterface->RequestRenderBuffer<uint32_t>(TileCountX * TileCountY * (MaxPointLightPerTile + 1), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	GSpotLightListBuffer = GraphicInterface->RequestRenderBuffer<uint32_t>(TileCountX * TileCountY * (MaxSpotLightPerTile + 1), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	GSpotLightListTransBuffer = GraphicInterface->RequestRenderBuffer<uint32_t>(TileCountX * TileCountY * (MaxSpotLightPerTile + 1), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
}

void UHDeferredShadingRenderer::RelaseRenderingBuffers()
{
	GraphicInterface->RequestReleaseRT(GSceneDiffuse);
	GraphicInterface->RequestReleaseRT(GSceneNormal);
	GraphicInterface->RequestReleaseRT(GSceneMaterial);
	GraphicInterface->RequestReleaseRT(GSceneResult);
	GraphicInterface->RequestReleaseRT(GSceneMip);
	GraphicInterface->RequestReleaseRT(GSceneDepth);
	GraphicInterface->RequestReleaseRT(GSceneTranslucentDepth);
	GraphicInterface->RequestReleaseRT(GHalfDepth);
	GraphicInterface->RequestReleaseRT(GHalfTranslucentDepth);
	GraphicInterface->RequestReleaseRT(GSceneVertexNormal);
	GraphicInterface->RequestReleaseRT(GSceneTranslucentVertexNormal);
	GraphicInterface->RequestReleaseRT(GPostProcessRT);
	GraphicInterface->RequestReleaseRT(GPreviousSceneResult);
	GraphicInterface->RequestReleaseRT(GOpaqueSceneResult);
	GraphicInterface->RequestReleaseRT(GMotionVectorRT);
	GraphicInterface->RequestReleaseRT(GQuarterBlurredScene);
	GraphicInterface->RequestReleaseRT(GGaussianFilterTempRT0);
	GraphicInterface->RequestReleaseRT(GGaussianFilterTempRT1);

	if (GraphicInterface->IsRayTracingEnabled())
	{
		GraphicInterface->RequestReleaseRT(GRTShadowResult);
		GraphicInterface->RequestReleaseRT(GRTSharedTextureRG16F);
	}

	// point light list needs to be resized, so release it here instead in ReleaseDataBuffers()
	UH_SAFE_RELEASE(GPointLightListBuffer);
	UH_SAFE_RELEASE(GPointLightListTransBuffer);
	UH_SAFE_RELEASE(GSpotLightListBuffer);
	UH_SAFE_RELEASE(GSpotLightListTransBuffer);
}

void UHDeferredShadingRenderer::CreateRenderPasses()
{
	// -------------------------------------------------------- Createing render pass after render pass is done -------------------------------------------------------- //
	std::vector<UHTexture*> GBufferTextures = { GSceneDiffuse ,GSceneNormal ,GSceneMaterial ,GSceneResult ,GSceneMip,GSceneVertexNormal };

	// depth prepass
	if (bEnableDepthPrePass)
	{
		DepthPassObj = GraphicInterface->CreateRenderPass(UHTransitionInfo(), GSceneDepth);
	}

	// create render pass based on output RT, render pass can be shared sometimes
	BasePassObj = GraphicInterface->CreateRenderPass(GBufferTextures, UHTransitionInfo(bEnableDepthPrePass ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR)
		, GSceneDepth);

	// sky need depth
	SkyboxPassObj = GraphicInterface->CreateRenderPass(GSceneResult
		, UHTransitionInfo(VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		, GSceneDepth);

	// translucent pass can share the same render pass obj as skybox pass
	TranslucentPassObj = SkyboxPassObj;

	// post processing render pass
	PostProcessPassObj[0] = GraphicInterface->CreateRenderPass(GSceneResult
		, UHTransitionInfo(VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
	PostProcessPassObj[1] = PostProcessPassObj[0];

	// motion pass, opaque and translucent can share the same RenderPass
	MotionCameraPassObj = GraphicInterface->CreateRenderPass(GMotionVectorRT, UHTransitionInfo());
	MotionOpaquePassObj = GraphicInterface->CreateRenderPass(GMotionVectorRT
		, UHTransitionInfo(VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		, GSceneDepth);

	std::vector<UHTexture*> TranslucentMotionTextures = { GMotionVectorRT , GSceneNormal };
	MotionTranslucentPassObj = GraphicInterface->CreateRenderPass(TranslucentMotionTextures
		, UHTransitionInfo(VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		, GSceneDepth);
}

void UHDeferredShadingRenderer::CreateRenderFrameBuffers()
{
	// collect image views for creaing one frame buffer
	std::vector<VkImageView> Views;
	Views.push_back(GSceneDiffuse->GetImageView());
	Views.push_back(GSceneNormal->GetImageView());
	Views.push_back(GSceneMaterial->GetImageView());
	Views.push_back(GSceneResult->GetImageView());
	Views.push_back(GSceneMip->GetImageView());
	Views.push_back(GSceneVertexNormal->GetImageView());
	Views.push_back(GSceneDepth->GetImageView());

	// depth frame buffer
	if (bEnableDepthPrePass)
	{
		DepthPassObj.FrameBuffer = GraphicInterface->CreateFrameBuffer(GSceneDepth->GetImageView(), DepthPassObj.RenderPass, RenderResolution);
	}

	// create frame buffer, some of them can be shared, especially when the output target is the same
	BasePassObj.FrameBuffer = GraphicInterface->CreateFrameBuffer(Views, BasePassObj.RenderPass, RenderResolution);

	// sky pass need depth
	Views = { GSceneResult->GetImageView() , GSceneDepth->GetImageView() };
	SkyboxPassObj.FrameBuffer = GraphicInterface->CreateFrameBuffer(Views, SkyboxPassObj.RenderPass, RenderResolution);

	// translucent pass can share the same frame buffer as skybox pass
	TranslucentPassObj.FrameBuffer = SkyboxPassObj.FrameBuffer;

	// post process pass, use two buffer and blit each other
	PostProcessPassObj[0].FrameBuffer = GraphicInterface->CreateFrameBuffer(GPostProcessRT->GetImageView(), PostProcessPassObj[0].RenderPass, RenderResolution);
	PostProcessPassObj[1].FrameBuffer = GraphicInterface->CreateFrameBuffer(GSceneResult->GetImageView(), PostProcessPassObj[1].RenderPass, RenderResolution);

	// motion pass framebuffer
	MotionCameraPassObj.FrameBuffer = GraphicInterface->CreateFrameBuffer(GMotionVectorRT->GetImageView(), MotionCameraPassObj.RenderPass, RenderResolution);

	// the opaque depth will be copied to translucent depth before motion opaque pass kicks off
	Views = { GMotionVectorRT->GetImageView(), GSceneTranslucentDepth->GetImageView() };
	MotionOpaquePassObj.FrameBuffer = GraphicInterface->CreateFrameBuffer(Views, MotionOpaquePassObj.RenderPass, RenderResolution);

	Views = { GMotionVectorRT->GetImageView(), GSceneTranslucentVertexNormal->GetImageView(), GSceneTranslucentDepth->GetImageView() };
	MotionTranslucentPassObj.FrameBuffer = GraphicInterface->CreateFrameBuffer(Views, MotionTranslucentPassObj.RenderPass, RenderResolution);
}

void UHDeferredShadingRenderer::ReleaseRenderPassObjects()
{
	VkDevice LogicalDevice = GraphicInterface->GetLogicalDevice();

	if (bEnableDepthPrePass)
	{
		DepthPassObj.Release(LogicalDevice);
	}

	BasePassObj.Release(LogicalDevice);
	SkyboxPassObj.Release(LogicalDevice);
	MotionCameraPassObj.Release(LogicalDevice);
	MotionOpaquePassObj.Release(LogicalDevice);
	MotionTranslucentPassObj.Release(LogicalDevice);
	PostProcessPassObj[0].ReleaseRenderPass(LogicalDevice);

	for (UHRenderPassObject& P : PostProcessPassObj)
	{
		P.ReleaseFrameBuffer(LogicalDevice);
	}
}

void UHDeferredShadingRenderer::CreateDataBuffers()
{
	if (CurrentScene == nullptr)
	{
		UHE_LOG(L"Scene is not set!\n");
		return;
	}

	// create constants and buffers
	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		GSystemConstantBuffer[Idx] = GraphicInterface->RequestRenderBuffer<UHSystemConstants>(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

		// create twice large buffer in editor intentionally, so appending more renderers don't need a re-allocation that fast
#if WITH_EDITOR
		const size_t RendererCount = CurrentScene->GetAllRendererCount() * 2;
#else
		const size_t RendererCount = CurrentScene->GetAllRendererCount();
#endif
		GObjectConstantBuffer[Idx] = GraphicInterface->RequestRenderBuffer<UHObjectConstants>(RendererCount, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		ObjectConstantsCPU.resize(RendererCount);

		GDirectionalLightBuffer[Idx] = GraphicInterface->RequestRenderBuffer<UHDirectionalLightConstants>(CurrentScene->GetDirLightCount(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		DirLightConstantsCPU.resize(CurrentScene->GetDirLightCount());

		GPointLightBuffer[Idx] = GraphicInterface->RequestRenderBuffer<UHPointLightConstants>(CurrentScene->GetPointLightCount(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		PointLightConstantsCPU.resize(CurrentScene->GetPointLightCount());

		GSpotLightBuffer[Idx] = GraphicInterface->RequestRenderBuffer<UHSpotLightConstants>(CurrentScene->GetSpotLightCount(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		SpotLightConstantsCPU.resize(CurrentScene->GetSpotLightCount());
	}

	GSH9Data = GraphicInterface->RequestRenderBuffer<UHSphericalHarmonicData>(1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
}

void UHDeferredShadingRenderer::CreateThreadObjects()
{
#if WITH_EDITOR
	for (int32_t Idx = 0; Idx < UHRenderPassMax; Idx++)
	{
		GPUTimeQueries[Idx] = GraphicInterface->RequestGPUQuery(2, VK_QUERY_TYPE_TIMESTAMP);
	}
	ThreadDrawCalls.resize(NumWorkerThreads);
#endif

	// create parallel submitter
	DepthParallelSubmitter.Initialize(GraphicInterface->GetLogicalDevice(), GraphicInterface->GetQueueFamily(), NumWorkerThreads);
	BaseParallelSubmitter.Initialize(GraphicInterface->GetLogicalDevice(), GraphicInterface->GetQueueFamily(), NumWorkerThreads);
	MotionOpaqueParallelSubmitter.Initialize(GraphicInterface->GetLogicalDevice(), GraphicInterface->GetQueueFamily(), NumWorkerThreads);
	MotionTranslucentParallelSubmitter.Initialize(GraphicInterface->GetLogicalDevice(), GraphicInterface->GetQueueFamily(), NumWorkerThreads);
	TranslucentParallelSubmitter.Initialize(GraphicInterface->GetLogicalDevice(), GraphicInterface->GetQueueFamily(), NumWorkerThreads);

	// init threads, it will wait at the beginning
	RenderThread = MakeUnique<UHThread>();
	RenderThread->BeginThread(std::thread(&UHDeferredShadingRenderer::RenderThreadLoop, this), GRenderThreadAffinity);
	WorkerThreads.resize(NumWorkerThreads);

	for (int32_t Idx = 0; Idx < NumWorkerThreads; Idx++)
	{
		WorkerThreads[Idx] = MakeUnique<UHThread>();
		WorkerThreads[Idx]->BeginThread(std::thread(&UHDeferredShadingRenderer::WorkerThreadLoop, this, Idx), GWorkerThreadAffinity + Idx);
	}
}

void UHDeferredShadingRenderer::ReleaseDataBuffers()
{
	// vk destroy functions
	for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		UH_SAFE_RELEASE(GSystemConstantBuffer[Idx]);
		UH_SAFE_RELEASE(GObjectConstantBuffer[Idx]);
		UH_SAFE_RELEASE(GDirectionalLightBuffer[Idx]);
		UH_SAFE_RELEASE(GPointLightBuffer[Idx]);
		UH_SAFE_RELEASE(GSpotLightBuffer[Idx]);

		if (GraphicInterface->IsRayTracingEnabled())
		{
			UH_SAFE_RELEASE(TopLevelAS[Idx]);
		}
	}

	UH_SAFE_RELEASE(IndicesTypeBuffer);
	UH_SAFE_RELEASE(GSH9Data);
}

void UHDeferredShadingRenderer::UpdateTextureDescriptors()
{
	// ------------------------------------------------ Bindless table update
	std::vector<UHTexture*> Texes;

	// bind textures from assets
	const std::vector<UHTexture2D*> AssetTextures = AssetManagerInterface->GetReferencedTexture2Ds();
	for (size_t Idx = 0; Idx < AssetTextures.size(); Idx++)
	{
		Texes.push_back(AssetTextures[Idx]);
	}

	// bind necessary textures from system
	Texes.push_back(GOpaqueSceneResult);
	GRefractionClearIndex = (int32_t)Texes.size() - 1;

	Texes.push_back(GQuarterBlurredScene);
	GRefractionBlurredIndex = (int32_t)Texes.size() - 1;

	size_t ReferencedSize = Texes.size();
	while (ReferencedSize < GTextureTableSize)
	{
		// fill with null image for non-referenced textures
		Texes.push_back(nullptr);
		ReferencedSize++;
	}
	TextureTable->BindImage(Texes, 0);

	// bind sampler table
	std::vector<UHSampler*> Samplers = GraphicInterface->GetSamplers();
	ReferencedSize = Samplers.size();
	while (ReferencedSize < GSamplerTableSize)
	{
		// fill with the first sampler as desciptor sampler can't be null
		Samplers.push_back(Samplers[0]);
		ReferencedSize++;
	}

	SamplerTable->BindSampler(Samplers, 0);
}

#if WITH_EDITOR
template <typename T>
void SafeReleaseShaderPtr(std::unordered_map<int32_t, T>& InMap, const int32_t InIndex, bool bDescriptorOnly)
{
	if (InMap.find(InIndex) != InMap.end())
	{
		InMap[InIndex]->Release(bDescriptorOnly);
	}
}

UHDeferredShadingRenderer* UHDeferredShadingRenderer::GetRendererEditorOnly()
{
	return SceneRendererEditorOnly;
}

void UHDeferredShadingRenderer::RefreshMaterialShaders(UHMaterial* Mat, bool bNeedReassignRendererGroup)
{
	// refresh material shader if necessary
	UHMaterialCompileFlag CompileFlag = Mat->GetCompileFlag();
	if (CompileFlag == UpToDate)
	{
		return;
	}

	GraphicInterface->WaitGPU();
	CheckTextureReference(Mat);

	const bool bIsOpaque = Mat->IsOpaque();

	for (UHObject* RendererObj : Mat->GetReferenceObjects())
	{
		if (UHMeshRendererComponent* Renderer = static_cast<UHMeshRendererComponent*>(RendererObj))
		{
			ResetMaterialShaders(Renderer, CompileFlag, bIsOpaque, bNeedReassignRendererGroup);
		}
	}

	// recreate shader if need to assign renderer group again
	if (bNeedReassignRendererGroup || CompileFlag == FullCompileResave || CompileFlag == FullCompileTemporary)
	{
		for (UHObject* RendererObj : Mat->GetReferenceObjects())
		{
			if (UHMeshRendererComponent* Renderer = static_cast<UHMeshRendererComponent*>(RendererObj))
			{
				RecreateMaterialShaders(Renderer, Mat);
			}
		}
	}

	// update hit group shader as well
	if (GraphicInterface->IsRayTracingEnabled() && CompileFlag != BindOnly && CompileFlag != StateChangedOnly)
	{
		RecreateRTShaders(Mat, false);
	}

	Mat->SetCompileFlag(UpToDate);

	// mark render dirties for re-uploading constant and updating TLAS
	Mat->SetRenderDirties(true);

	UpdateDescriptors();
}

void UHDeferredShadingRenderer::OnRendererMaterialChanged(UHMeshRendererComponent* InRenderer, UHMaterial* OldMat, UHMaterial* NewMat)
{
	// callback function when the renderer's material changed
	// the design shouldn't allow nullptr here
	assert(OldMat != nullptr);
	assert(NewMat != nullptr);

	// wait GPU finished
	GraphicInterface->WaitGPU();

	// remove old reference
	OldMat->RemoveReferenceObject(InRenderer);

	const bool bNeedReassignGroup = UHMaterial::IsDifferentBlendGroup(OldMat, NewMat);

	// set new material cache directly if it doesn't be reassigned
	// other wise remove the old renderer and recreate new one
	const int32_t RendererBufferIndex = InRenderer->GetBufferDataIndex();

	if (!bNeedReassignGroup)
	{
		if (NewMat->IsOpaque())
		{
			if (bEnableDepthPrePass)
			{
				DepthPassShaders[RendererBufferIndex]->SetNewMaterialCache(NewMat);
			}
			BasePassShaders[RendererBufferIndex]->SetNewMaterialCache(NewMat);
			MotionOpaqueShaders[RendererBufferIndex]->SetNewMaterialCache(NewMat);
		}
		else
		{
			MotionTranslucentShaders[RendererBufferIndex]->SetNewMaterialCache(NewMat);
			TranslucentPassShaders[RendererBufferIndex]->SetNewMaterialCache(NewMat);
		}

		// re-bind the parameter
		ResetMaterialShaders(InRenderer, BindOnly, NewMat->IsOpaque(), false);
	}
	else
	{
		// state changed (different blendmode), need a recreate
		ResetMaterialShaders(InRenderer, RendererMaterialChanged, OldMat->IsOpaque(), true);
		RecreateMaterialShaders(InRenderer, NewMat);
	}

	NewMat->AddReferenceObject(InRenderer);
	UpdateDescriptors();
}

void UHDeferredShadingRenderer::ResetMaterialShaders(UHMeshRendererComponent* InMeshRenderer, UHMaterialCompileFlag CompileFlag, bool bIsOpaque, bool bNeedReassignRendererGroup)
{
	const int32_t RendererBufferIndex = InMeshRenderer->GetBufferDataIndex();
	const bool bReleaseDescriptorOnly = CompileFlag == RendererMaterialChanged;

	if (bNeedReassignRendererGroup || CompileFlag == RendererMaterialChanged || CompileFlag == FullCompileResave
		|| CompileFlag == FullCompileTemporary)
	{
		// release shaders if it's re-compiling
		if (bEnableDepthPrePass)
		{
			SafeReleaseShaderPtr(DepthPassShaders, RendererBufferIndex, bReleaseDescriptorOnly);
		}

		SafeReleaseShaderPtr(BasePassShaders, RendererBufferIndex, bReleaseDescriptorOnly);
		SafeReleaseShaderPtr(MotionOpaqueShaders, RendererBufferIndex, bReleaseDescriptorOnly);
		SafeReleaseShaderPtr(MotionTranslucentShaders, RendererBufferIndex, bReleaseDescriptorOnly);
		SafeReleaseShaderPtr(TranslucentPassShaders, RendererBufferIndex, bReleaseDescriptorOnly);
	}
	else if (CompileFlag == BindOnly)
	{
		// bind only
		if (bIsOpaque)
		{
			if (bEnableDepthPrePass)
			{
				DepthPassShaders[RendererBufferIndex]->BindParameters(InMeshRenderer);
			}

			BasePassShaders[RendererBufferIndex]->BindParameters(InMeshRenderer);
			MotionOpaqueShaders[RendererBufferIndex]->BindParameters(InMeshRenderer);
		}
		else
		{
			MotionTranslucentShaders[RendererBufferIndex]->BindParameters(InMeshRenderer);
			TranslucentPassShaders[RendererBufferIndex]->BindParameters(InMeshRenderer);
		}
	}
	else if (CompileFlag == StateChangedOnly)
	{
		// re-create state only
		if (bIsOpaque)
		{
			if (bEnableDepthPrePass)
			{
				DepthPassShaders[RendererBufferIndex]->RecreateMaterialState();
			}
			BasePassShaders[RendererBufferIndex]->RecreateMaterialState();
			MotionOpaqueShaders[RendererBufferIndex]->RecreateMaterialState();
		}
		else
		{
			MotionTranslucentShaders[RendererBufferIndex]->RecreateMaterialState();
			TranslucentPassShaders[RendererBufferIndex]->RecreateMaterialState();
		}
	}

	// re-assign renderer and erase old shader instances
	if (bNeedReassignRendererGroup)
	{
		CurrentScene->ReassignRenderer(InMeshRenderer);
		DepthPassShaders.erase(RendererBufferIndex);
		BasePassShaders.erase(RendererBufferIndex);
		MotionOpaqueShaders.erase(RendererBufferIndex);
		MotionTranslucentShaders.erase(RendererBufferIndex);
		TranslucentPassShaders.erase(RendererBufferIndex);
	}

	InMeshRenderer->SetRayTracingDirties(true);
}

void UHDeferredShadingRenderer::AppendMeshRenderers(const std::vector<UHMeshRendererComponent*> InRenderers)
{
	GraphicInterface->WaitGPU();
	for (UHMeshRendererComponent* MeshRenderer : InRenderers)
	{
		CurrentScene->AddMeshRenderer(MeshRenderer);
	}

	// data buffer recreating
	if (CurrentScene->GetAllRendererCount() > ObjectConstantsCPU.size())
	{
		const size_t RendererCount = CurrentScene->GetAllRendererCount() * 2;
		ObjectConstantsCPU.resize(RendererCount);
		for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
		{
			UH_SAFE_RELEASE(GObjectConstantBuffer[Idx]);
			GObjectConstantBuffer[Idx]->CreateBuffer(RendererCount, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		}
	}

	// do the same as initialization
	PrepareMeshes();
	PrepareTextures();

	for (UHMeshRendererComponent* Renderer : InRenderers)
	{
		UHMaterial* Mat = Renderer->GetMaterial();
		if (Mat && !Mat->GetMaterialUsages().bIsSkybox)
		{
			RecreateMaterialShaders(Renderer, Mat);
		}
	}

	if (GraphicInterface->IsRayTracingEnabled())
	{
		RecreateRTShaders(nullptr, true);
	}
	UpdateDescriptors();
}

#endif

void UHDeferredShadingRenderer::RecreateMaterialShaders(UHMeshRendererComponent* InMeshRenderer, UHMaterial* InMat)
{
	int32_t RendererBufferIndex = InMeshRenderer->GetBufferDataIndex();
	const std::vector<VkDescriptorSetLayout> BindlessLayouts = { TextureTable->GetDescriptorSetLayout(), SamplerTable->GetDescriptorSetLayout() };
	const bool bHasEnvCube = (GSkyLightCube != nullptr);

	if (InMat->IsOpaque())
	{
		if (bEnableDepthPrePass)
		{
			DepthPassShaders[RendererBufferIndex] = MakeUnique<UHDepthPassShader>(GraphicInterface, "DepthPassShader", DepthPassObj.RenderPass, InMat, BindlessLayouts);
		}

		BasePassShaders[RendererBufferIndex] = MakeUnique<UHBasePassShader>(GraphicInterface, "BasePassShader", BasePassObj.RenderPass, InMat, bEnableDepthPrePass
			, bHasEnvCube, BindlessLayouts);

		MotionOpaqueShaders[RendererBufferIndex] = MakeUnique<UHMotionObjectPassShader>(GraphicInterface, "MotionObjectShader", MotionOpaquePassObj.RenderPass, InMat, bEnableDepthPrePass, BindlessLayouts);
	}
	else
	{
		MotionTranslucentShaders[RendererBufferIndex] = MakeUnique<UHMotionObjectPassShader>(GraphicInterface, "MotionObjectShader", MotionTranslucentPassObj.RenderPass, InMat, bEnableDepthPrePass, BindlessLayouts);
		TranslucentPassShaders[RendererBufferIndex]
			= MakeUnique<UHTranslucentPassShader>(GraphicInterface, "TranslucentPassShader", TranslucentPassObj.RenderPass, InMat, bHasEnvCube, BindlessLayouts);
	}

	InMeshRenderer->SetRayTracingDirties(true);
}

void UHDeferredShadingRenderer::RecreateRTShaders(UHMaterial* InMat, bool bRecreateTable)
{
	if (bRecreateTable)
	{
		// buffer for storing mesh vertex and indices
		UH_SAFE_RELEASE(RTVertexTable);
		RTVertexTable = MakeUnique<UHRTVertexTable>(GraphicInterface, "RTVertexTable", RTInstanceCount);

		UH_SAFE_RELEASE(RTIndicesTable);
		RTIndicesTable = MakeUnique<UHRTIndicesTable>(GraphicInterface, "RTIndicesTable", RTInstanceCount);

		// buffer for storing index type, used in hit group shader
		UH_SAFE_RELEASE(RTIndicesTypeTable);
		RTIndicesTypeTable = MakeUnique<UHRTIndicesTypeTable>(GraphicInterface, "RTIndicesTypeTable");

		UH_SAFE_RELEASE(IndicesTypeBuffer);
		IndicesTypeBuffer = GraphicInterface->RequestRenderBuffer<int32_t>(RTInstanceCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

		// buffer for storing texture index
		UH_SAFE_RELEASE(RTMaterialDataTable);
		RTMaterialDataTable = MakeUnique<UHRTMaterialDataTable>(GraphicInterface, "RTIndicesTypeTable", static_cast<uint32_t>(CurrentScene->GetMaterialCount()));

		UH_SAFE_RELEASE(RTDefaultHitGroupShader);
		RTDefaultHitGroupShader = MakeUnique<UHRTDefaultHitGroupShader>(GraphicInterface, "RTDefaultHitGroupShader", CurrentScene->GetMaterials());
	}

	if (InMat)
	{
		RTDefaultHitGroupShader->UpdateHitShader(GraphicInterface, InMat);
	}

	const std::vector<VkDescriptorSetLayout> Layouts = { TextureTable->GetDescriptorSetLayout()
		, SamplerTable->GetDescriptorSetLayout()
		, RTVertexTable->GetDescriptorSetLayout()
		, RTIndicesTable->GetDescriptorSetLayout()
		, RTIndicesTypeTable->GetDescriptorSetLayout()
		, RTMaterialDataTable->GetDescriptorSetLayout() };

	UH_SAFE_RELEASE(RTShadowShader);
	RTShadowShader = MakeUnique<UHRTShadowShader>(GraphicInterface, "RTShadowShader", RTDefaultHitGroupShader->GetClosestShaders(), RTDefaultHitGroupShader->GetAnyHitShaders()
		, Layouts);
}