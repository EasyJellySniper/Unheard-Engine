#include "DeferredShadingRenderer.h"
#include "DescriptorHelper.h"

// all init/create/release implementations of DeferredShadingRenderer are put here

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
	, SceneDiffuse(nullptr)
	, SceneNormal(nullptr)
	, SceneMaterial(nullptr)
	, SceneResult(nullptr)
	, SceneMip(nullptr)
	, SceneDepth(nullptr)
	, SceneTranslucentDepth(nullptr)
	, SceneVertexNormal(nullptr)
	, SceneTranslucentVertexNormal(nullptr)
	, PointClampedSampler(nullptr)
	, LinearClampedSampler(nullptr)
	, AnisoClampedSampler(nullptr)
	, DefaultSamplerIndex(-1)
	, bEnableDepthPrePass(false)
	, PostProcessRT(nullptr)
	, PreviousSceneResult(nullptr)
	, PostProcessResultIdx(0)
	, bIsTemporalReset(true)
	, MotionVectorRT(nullptr)
	, PrevMotionVectorRT(nullptr)
	, RTShadowResult(nullptr)
	, RTTranslucentShadow(nullptr)
	, RTInstanceCount(0)
	, IndicesTypeBuffer(nullptr)
	, NumWorkerThreads(0)
	, RenderThread(nullptr)
	, bParallelSubmissionRT(false)
	, bVsyncRT(false)
	, bIsSwapChainResetGT(false)
	, bIsSwapChainResetRT(false)
	, ParallelTask(UHParallelTask::None)
#if WITH_DEBUG
	, DebugViewIndex(0)
	, RenderThreadTime(0)
	, DrawCalls(0)
#endif
{
	// init static array pointers
	for (size_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		TopLevelAS[Idx] = VK_NULL_HANDLE;
	}

	for (int32_t Idx = 0; Idx < NumOfPostProcessRT; Idx++)
	{
		PostProcessResults[Idx] = nullptr;
	}

	// init formats
	DiffuseFormat = VK_FORMAT_R8G8B8A8_SRGB;
	NormalFormat = VK_FORMAT_A2R10G10B10_UNORM_PACK32;
	SpecularFormat = VK_FORMAT_R8G8B8A8_UNORM;
	SceneResultFormat = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
	DepthFormat = VK_FORMAT_X8_D24_UNORM_PACK32;

	// half precision for motion vector
	MotionFormat = VK_FORMAT_R16G16_SFLOAT;

	// RT result format, R for distance to blocker, G for shadow strength
	RTShadowFormat = VK_FORMAT_R16G16_SFLOAT;

	// a buffer to store vertex normal (different than bump normal) and mip map level
	SceneMipFormat = VK_FORMAT_R16_SFLOAT;

#if WITH_DEBUG
	for (int32_t Idx = 0; Idx < UHRenderPassMax; Idx++)
	{
		GPUTimeQueries[Idx] = nullptr;
		GPUTimes[Idx] = 0.0f;
	}
#endif
}

bool UHDeferredShadingRenderer::Initialize(UHScene* InScene)
{
	// scene setup
	CurrentScene = InScene;
	PrepareMeshes();
	PrepareTextures();

	// config setup
	bEnableDepthPrePass = GraphicInterface->IsDepthPrePassEnabled();
	NumWorkerThreads = ConfigInterface->RenderingSetting().ParallelThreads;

	// shared sampler requests
	UHSamplerInfo PointClampedInfo(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	PointClampedInfo.MaxAnisotropy = 1;
	PointClampedSampler = GraphicInterface->RequestTextureSampler(PointClampedInfo);

	UHSamplerInfo LinearClampedInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	LinearClampedInfo.MaxAnisotropy = 1;
	LinearClampedSampler = GraphicInterface->RequestTextureSampler(LinearClampedInfo);

	LinearClampedInfo.MaxAnisotropy = 16;
	AnisoClampedSampler = GraphicInterface->RequestTextureSampler(LinearClampedInfo);
	DefaultSamplerIndex = UHUtilities::FindIndex(GraphicInterface->GetSamplers(), AnisoClampedSampler);

	const bool bIsRendererSuccess = InitQueueSubmitters();
	if (bIsRendererSuccess)
	{
		// create render pass stuffs
		CreateRenderPasses();
		CreateRenderingBuffers();
		PrepareRenderingShaders();

		// create data buffers
		CreateDataBuffers();

		// update descriptor binding
		UpdateDescriptors();

		// create thread objects
		CreateThreadObjects();

		// reserve enough space for renderers, save the allocation time
		OpaquesToRender.reserve(CurrentScene->GetOpaqueRenderers().size());
		TranslucentsToRender.reserve(CurrentScene->GetTranslucentRenderers().size());
	}

	return bIsRendererSuccess;
}

void UHDeferredShadingRenderer::Release()
{
	VkDevice LogicalDevice = GraphicInterface->GetLogicalDevice();

	// wait device to finish before release
	GraphicInterface->WaitGPU();

	// end render thread
	RenderThread->EndThread();
	RenderThread.reset();
	for (auto& WorkerThread : WorkerThreads)
	{
		WorkerThread->EndThread();
		WorkerThread.reset();
	}
	WorkerThreads.clear();

	// release GBuffers
	RelaseRenderingBuffers();

	// release data buffers
	ReleaseDataBuffers();

	// release descriptors & render pass stuffs
	ReleaseRenderPassObjects();
	ReleaseShaders();

	// vk destroy functions
	for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		UH_SAFE_RELEASE(SystemConstantsGPU[Idx]);
		SystemConstantsGPU[Idx].reset();

		UH_SAFE_RELEASE(ObjectConstantsGPU[Idx]);
		ObjectConstantsGPU[Idx].reset();

		UH_SAFE_RELEASE(DirectionalLightBuffer[Idx]);
		DirectionalLightBuffer[Idx].reset();

		if (GraphicInterface->IsRayTracingEnabled())
		{
			UH_SAFE_RELEASE(TopLevelAS[Idx]);
			TopLevelAS[Idx].reset();
		}
	}

	UH_SAFE_RELEASE(IndicesTypeBuffer);
	IndicesTypeBuffer.reset();

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

	if (Renderers.size() == 0)
	{
		return;
	}

	// needs the cmd buffer
	VkCommandBuffer CreationCmd = GraphicInterface->BeginOneTimeCmd();

	for (const UHMeshRendererComponent* Renderer : Renderers)
	{
		UHMesh* Mesh = Renderer->GetMesh();
		UHMaterial* Mat = Renderer->GetMaterial();
		Mesh->CreateGPUBuffers(GraphicInterface);

		if (!Mat->IsSkybox())
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
		TopLevelAS[Idx] = GraphicInterface->RequestAccelerationStructure();
		RTInstanceCount = TopLevelAS[Idx]->CreateTopAS(Renderers, CreationCmd);
	}
	GraphicInterface->EndOneTimeCmd(CreationCmd);

	// release CPU mesh data after uploading
	for (const UHMeshRendererComponent* Renderer : Renderers)
	{
		Renderer->GetMesh()->ReleaseCPUMeshData();
	}

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
	UHGraphicBuilder GraphBuilder(GraphicInterface, CreationCmd);

	for (const std::string RegisteredTexture : InMat->GetRegisteredTextureNames())
	{
		UHTexture2D* Tex = AssetManagerInterface->GetTexture2D(RegisteredTexture);
		if (Tex)
		{
			Tex->UploadToGPU(GraphicInterface, CreationCmd, GraphBuilder);
			Tex->GenerateMipMaps(GraphicInterface, CreationCmd, GraphBuilder);
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
	UHGraphicBuilder GraphBuilder(GraphicInterface, CreationCmd);
	// Step2: Build all cubemaps in use
	for (const UHMeshRendererComponent* Renderer : CurrentScene->GetAllRenderers())
	{
		UHMaterial* Mat = Renderer->GetMaterial();
		if (Mat && Mat->GetSystemTex(UHSystemTextureType::SkyCube))
		{
			UHTextureCube* Cube = static_cast<UHTextureCube*>(Mat->GetSystemTex(UHSystemTextureType::SkyCube));
			Cube->Build(GraphicInterface, CreationCmd, GraphBuilder);
		}
	}

	if (const UHMeshRendererComponent* SkyRenderer = CurrentScene->GetSkyboxRenderer())
	{
		if (SkyRenderer->GetMaterial() && SkyRenderer->GetMaterial()->GetSystemTex(UHSystemTextureType::SkyCube))
		{
			UHTextureCube* Cube = static_cast<UHTextureCube*>(SkyRenderer->GetMaterial()->GetSystemTex(UHSystemTextureType::SkyCube));
			Cube->Build(GraphicInterface, CreationCmd, GraphBuilder);
		}
	}

	GraphicInterface->EndOneTimeCmd(CreationCmd);
}

void UHDeferredShadingRenderer::PrepareRenderingShaders()
{
	// bindless table
	TextureTable = UHTextureTable(GraphicInterface, "TextureTable", static_cast<uint32_t>(AssetManagerInterface->GetTexture2Ds().size()));
	SamplerTable = UHSamplerTable(GraphicInterface, "SamplerTable", static_cast<uint32_t>(GraphicInterface->GetSamplers().size()));
	const std::vector<VkDescriptorSetLayout> BindlessLayouts = { TextureTable.GetDescriptorSetLayout(), SamplerTable.GetDescriptorSetLayout()};

	// depth pass shader, opaque only
	if (bEnableDepthPrePass)
	{
		for (const UHMeshRendererComponent* Renderer : CurrentScene->GetOpaqueRenderers())
		{
			UHMaterial* Mat = Renderer->GetMaterial();
			if (Mat && DepthPassShaders.find(Renderer->GetBufferDataIndex()) == DepthPassShaders.end())
			{
				DepthPassShaders[Renderer->GetBufferDataIndex()] = UHDepthPassShader(GraphicInterface, "DepthPassShader", DepthPassObj.RenderPass, Mat, BindlessLayouts);
			}
		}
	}

	// base pass shader, opaque only
	for (const UHMeshRendererComponent* Renderer : CurrentScene->GetOpaqueRenderers())
	{
		UHMaterial* Mat = Renderer->GetMaterial();
		if (Mat && BasePassShaders.find(Renderer->GetBufferDataIndex()) == BasePassShaders.end())
		{
			BasePassShaders[Renderer->GetBufferDataIndex()] = UHBasePassShader(GraphicInterface, "BasePassShader", BasePassObj.RenderPass, Mat, bEnableDepthPrePass, BindlessLayouts);
		}
	}

	// light pass shaders
	LightPassShader = UHLightPassShader(GraphicInterface, "LightPassShader", RTInstanceCount);

	// sky pass shader
	SkyPassShader = UHSkyPassShader(GraphicInterface, "SkyPassShader", SkyboxPassObj.RenderPass);

	// motion pass shader
	MotionCameraShader = UHMotionCameraPassShader(GraphicInterface, "MotionCameraPassShader", MotionCameraPassObj.RenderPass);
	for (const UHMeshRendererComponent* Renderer : CurrentScene->GetAllRenderers())
	{
		UHMaterial* Mat = Renderer->GetMaterial();
		if (Mat == nullptr || Mat->IsSkybox())
		{
			continue;
		}

		const bool bIsOpaque = Mat->GetBlendMode() <= UHBlendMode::Masked;
		const int32_t RendererIdx = Renderer->GetBufferDataIndex();
		if (bIsOpaque && MotionOpaqueShaders.find(RendererIdx) == MotionOpaqueShaders.end())
		{
			MotionOpaqueShaders[RendererIdx]
				= UHMotionObjectPassShader(GraphicInterface, "MotionObjectShader", MotionOpaquePassObj.RenderPass, Mat, bEnableDepthPrePass, BindlessLayouts);
		}
		else if (MotionTranslucentShaders.find(RendererIdx) == MotionTranslucentShaders.end())
		{
			MotionTranslucentShaders[RendererIdx]
				= UHMotionObjectPassShader(GraphicInterface, "MotionObjectShader", MotionTranslucentPassObj.RenderPass, Mat, bEnableDepthPrePass, BindlessLayouts);
		}
	}

	// translucent pass shader
	for (const UHMeshRendererComponent* Renderer : CurrentScene->GetTranslucentRenderers())
	{
		UHMaterial* Mat = Renderer->GetMaterial();
		if (Mat && TranslucentPassShaders.find(Renderer->GetBufferDataIndex()) == TranslucentPassShaders.end())
		{
			TranslucentPassShaders[Renderer->GetBufferDataIndex()] 
				= UHTranslucentPassShader(GraphicInterface, "TranslucentPassShader", TranslucentPassObj.RenderPass, Mat, BindlessLayouts);
		}
	}

	// post processing shaders
	TemporalAAShader = UHTemporalAAShader(GraphicInterface, "TemporalAAShader");
	ToneMapShader = UHToneMappingShader(GraphicInterface, "ToneMapShader", PostProcessPassObj[0].RenderPass);

	// RT shaders
	if (GraphicInterface->IsRayTracingEnabled())
	{
		// buffer for storing mesh vertex and indices
		RTVertexTable = UHRTVertexTable(GraphicInterface, "RTVertexTable", RTInstanceCount);
		RTIndicesTable = UHRTIndicesTable(GraphicInterface, "RTIndicesTable", RTInstanceCount);

		// buffer for storing index type, used in hit group shader
		RTIndicesTypeTable = UHRTIndicesTypeTable(GraphicInterface, "RTIndicesTypeTable");
		IndicesTypeBuffer = GraphicInterface->RequestRenderBuffer<int32_t>();
		IndicesTypeBuffer->CreateBuffer(RTInstanceCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

		// buffer for storing texture index
		RTMaterialDataTable = UHRTMaterialDataTable(GraphicInterface, "RTIndicesTypeTable", static_cast<uint32_t>(CurrentScene->GetMaterialCount()));

		RTDefaultHitGroupShader = UHRTDefaultHitGroupShader(GraphicInterface, "RTDefaultHitGroupShader", CurrentScene->GetMaterials());

		// also send texture & VB/IB layout to RT shadow shader
		const std::vector<VkDescriptorSetLayout> Layouts = { TextureTable.GetDescriptorSetLayout()
			, SamplerTable.GetDescriptorSetLayout()
			, RTVertexTable.GetDescriptorSetLayout()
			, RTIndicesTable.GetDescriptorSetLayout()
			, RTIndicesTypeTable.GetDescriptorSetLayout()
			, RTMaterialDataTable.GetDescriptorSetLayout()};

		// shadow shader
		RTShadowShader = UHRTShadowShader(GraphicInterface, "RTShadowShader", RTDefaultHitGroupShader.GetClosestShaders(), RTDefaultHitGroupShader.GetAnyHitShaders()
			, Layouts);

		// soft rt shadow shader
		SoftRTShadowShader = UHSoftRTShadowShader(GraphicInterface, "SoftRTShadowShader");
	}

#if WITH_DEBUG
	DebugViewShader = UHDebugViewShader(GraphicInterface, "DebugViewShader", PostProcessPassObj[0].RenderPass);
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

	// ------------------------------------------------ Bindless table update
	// bind texture table
	std::vector<UHTexture*> Texes(AssetManagerInterface->GetReferencedTexture2Ds().size());
	for (size_t Idx = 0; Idx < Texes.size(); Idx++)
	{
		Texes[Idx] = AssetManagerInterface->GetReferencedTexture2Ds()[Idx];
	}

	size_t ReferencedSize = Texes.size();
	while (ReferencedSize < AssetManagerInterface->GetTexture2Ds().size())
	{
		// fill with null image for non-referenced textures
		Texes.push_back(nullptr);
		ReferencedSize++;
	}

	TextureTable.BindImage(Texes, 0);

	// bind sampler table
	std::vector<UHSampler*> Samplers = GraphicInterface->GetSamplers();
	SamplerTable.BindSampler(Samplers, 0);

	// ------------------------------------------------ Depth pass descriptor update
	if (bEnableDepthPrePass)
	{
		for (const UHMeshRendererComponent* Renderer : CurrentScene->GetOpaqueRenderers())
		{
	#if WITH_DEBUG
			if (DepthPassShaders.find(Renderer->GetBufferDataIndex()) == DepthPassShaders.end())
			{
				// unlikely to happen, but printing a message for debug
				UHE_LOG(L"[UpdateDescriptors] Can't find depth pass shader for renderer\n");
				continue;
			}
	#endif

			UHDepthPassShader& DepthShader = DepthPassShaders[Renderer->GetBufferDataIndex()];
			DepthShader.BindParameters(SystemConstantsGPU, ObjectConstantsGPU, Renderer);
		}
	}

	// ------------------------------------------------ Base pass descriptor update
	for (const UHMeshRendererComponent* Renderer : CurrentScene->GetOpaqueRenderers())
	{
	#if WITH_DEBUG
		if (BasePassShaders.find(Renderer->GetBufferDataIndex()) == BasePassShaders.end())
		{
			// unlikely to happen, but printing a message for debug
			UHE_LOG(L"[UpdateDescriptors] Can't find base pass shader for renderer\n");
			continue;
		}
	#endif

		UHBasePassShader& BaseShader = BasePassShaders[Renderer->GetBufferDataIndex()];
		BaseShader.BindParameters(SystemConstantsGPU, ObjectConstantsGPU, Renderer);
	}

	// ------------------------------------------------ Lighting pass descriptor update
	const std::vector<UHTexture*> GBuffers = { SceneDiffuse, SceneNormal, SceneMaterial, SceneDepth, SceneMip, SceneVertexNormal };
	LightPassShader.BindParameters(SystemConstantsGPU, DirectionalLightBuffer, GBuffers, SceneResult, LinearClampedSampler
		, RTInstanceCount, RTShadowResult);

	// ------------------------------------------------ sky pass descriptor update
	if (const UHMeshRendererComponent* SkyRenderer = CurrentScene->GetSkyboxRenderer())
	{
		SkyPassShader.BindParameters(SystemConstantsGPU, ObjectConstantsGPU, SkyRenderer);
	}

	// ------------------------------------------------ motion pass descriptor update
	MotionCameraShader.BindParameters(SystemConstantsGPU, SceneDepth, PointClampedSampler);

	for (const UHMeshRendererComponent* Renderer : CurrentScene->GetAllRenderers())
	{
		UHMaterial* Mat = Renderer->GetMaterial();
		if (Mat == nullptr || Mat->IsSkybox())
		{
			continue;
		}

		const bool bIsOpaque = Mat->GetBlendMode() <= UHBlendMode::Masked;
		const int32_t RendererIdx = Renderer->GetBufferDataIndex();
		if (bIsOpaque)
		{
			if (MotionOpaqueShaders.find(RendererIdx) == MotionOpaqueShaders.end())
			{
				// unlikely to happen, but printing a message for debug
				UHE_LOG(L"[UpdateDescriptors] Can't find motion opaque pass shader for renderer\n");
				continue;
			}

			UHMotionObjectPassShader& MotionObjectShader = MotionOpaqueShaders[RendererIdx];
			MotionObjectShader.BindParameters(SystemConstantsGPU, ObjectConstantsGPU, Renderer);
		}
		else
		{
			if (MotionTranslucentShaders.find(RendererIdx) == MotionTranslucentShaders.end())
			{
				// unlikely to happen, but printing a message for debug
				UHE_LOG(L"[UpdateDescriptors] Can't find motion translucent pass shader for renderer\n");
				continue;
			}

			UHMotionObjectPassShader& MotionTranslucentShader = MotionTranslucentShaders[RendererIdx];
			MotionTranslucentShader.BindParameters(SystemConstantsGPU, ObjectConstantsGPU, Renderer);
		}
	}

	// ------------------------------------------------ translucent pass descriptor update
	for (const UHMeshRendererComponent* Renderer : CurrentScene->GetTranslucentRenderers())
	{
#if WITH_DEBUG
		if (TranslucentPassShaders.find(Renderer->GetBufferDataIndex()) == TranslucentPassShaders.end())
		{
			// unlikely to happen, but printing a message for debug
			UHE_LOG(L"[UpdateDescriptors] Can't find base pass shader for renderer\n");
			continue;
		}
#endif

		UHTranslucentPassShader& TranslucentShader = TranslucentPassShaders[Renderer->GetBufferDataIndex()];
		TranslucentShader.BindParameters(SystemConstantsGPU, ObjectConstantsGPU, DirectionalLightBuffer
			, RTTranslucentShadow, LinearClampedSampler, Renderer);
	}

	// ------------------------------------------------ post process pass descriptor update
	// when binding post processing input, be sure to bind it alternately, the two RT will keep bliting to each other
	// the post process RT binding will be in PostProcessRendering.cpp
	ToneMapShader.BindSampler(LinearClampedSampler, 1);

	TemporalAAShader.BindParameters(SystemConstantsGPU, PreviousSceneResult, MotionVectorRT, PrevMotionVectorRT
		, LinearClampedSampler);

	// ------------------------------------------------ ray tracing pass descriptor update
	if (GraphicInterface->IsRayTracingEnabled() && TopLevelAS[0] && RTInstanceCount > 0)
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
			if (!Mat || Mat->IsSkybox() || !Mesh)
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
		RTVertexTable.BindStorage(UVs, 0);
		RTIndicesTable.BindStorage(BufferInfos, 0);

		// upload index types
		IndicesTypeBuffer->UploadAllData(IndexTypes.data());
		RTIndicesTypeTable.BindStorage(IndicesTypeBuffer.get(), 0, 0, true);

		// collect and bind texture index data
		std::vector<UHRenderBuffer<UHMaterialData>*> TextureIndexData;
		const std::vector<UHMaterial*>& AllMaterials = CurrentScene->GetMaterials();
		for (size_t Idx = 0; Idx < AllMaterials.size(); Idx++)
		{
			TextureIndexData.push_back(AllMaterials[Idx]->GetRTMaterialDataGPU());
		}
		RTMaterialDataTable.BindStorage(TextureIndexData, 0);

		// bind RT shadow parameters
		RTShadowShader.BindParameters(SystemConstantsGPU, RTShadowResult, RTTranslucentShadow, DirectionalLightBuffer
			, SceneMip, SceneDepth, SceneTranslucentDepth, SceneVertexNormal, SceneTranslucentVertexNormal, LinearClampedSampler);

		// bind soft shadow parameters
		SoftRTShadowShader.BindParameters(SystemConstantsGPU, RTShadowResult, RTTranslucentShadow, SceneDepth
			, SceneTranslucentDepth, SceneMip, LinearClampedSampler);
	}

	// ------------------------------------------------ debug view pass descriptor update
#if WITH_DEBUG
	if (GraphicInterface->IsRayTracingEnabled() && RTInstanceCount > 0)
	{
		DebugViewShader.BindImage(RTShadowResult, 0);
	}
	DebugViewShader.BindSampler(PointClampedSampler, 1);
#endif
}

void UHDeferredShadingRenderer::ReleaseShaders()
{
	if (bEnableDepthPrePass)
	{
		for (auto& DepthShader : DepthPassShaders)
		{
			DepthShader.second.Release();
		}
		DepthPassShaders.clear();
	}

	for (auto& BaseShader : BasePassShaders)
	{
		BaseShader.second.Release();
	}
	BasePassShaders.clear();

	for (auto& MotionShader : MotionOpaqueShaders)
	{
		MotionShader.second.Release();
	}
	MotionOpaqueShaders.clear();

	for (auto& MotionShader : MotionTranslucentShaders)
	{
		MotionShader.second.Release();
	}
	MotionTranslucentShaders.clear();

	for (auto& TranslucentShader : TranslucentPassShaders)
	{
		TranslucentShader.second.Release();
	}
	TranslucentPassShaders.clear();

	LightPassShader.Release();
	SkyPassShader.Release();
	MotionCameraShader.Release();
	TemporalAAShader.Release();
	ToneMapShader.Release();

	if (GraphicInterface->IsRayTracingEnabled())
	{
		RTDefaultHitGroupShader.Release();
		RTShadowShader.Release();
		RTVertexTable.Release();
		RTIndicesTable.Release();
		RTIndicesTypeTable.Release();
		RTMaterialDataTable.Release();
		SoftRTShadowShader.Release();
	}

	TextureTable.Release();
	SamplerTable.Release();

#if WITH_DEBUG
	DebugViewShader.Release();
#endif
}

void UHDeferredShadingRenderer::CreateRenderingBuffers()
{
	// create GBuffer
	RenderResolution.width = ConfigInterface->RenderingSetting().RenderWidth;
	RenderResolution.height = ConfigInterface->RenderingSetting().RenderHeight;

	// color buffer (A channel as AO)
	SceneDiffuse = GraphicInterface->RequestRenderTexture("GBufferA", RenderResolution, DiffuseFormat, false);

	// normal buffer
	SceneNormal = GraphicInterface->RequestRenderTexture("GBufferB", RenderResolution, NormalFormat, true);

	// material buffer (M/R/S)
	SceneMaterial = GraphicInterface->RequestRenderTexture("GBufferC", RenderResolution, SpecularFormat, true);

	// scene result in HDR
	SceneResult = GraphicInterface->RequestRenderTexture("SceneResult", RenderResolution, SceneResultFormat, true, true);

	// uv grad buffer
	SceneMip = GraphicInterface->RequestRenderTexture("SceneMip", RenderResolution, SceneMipFormat, true);

	// depth buffer, also needs another depth buffer with translucent
	SceneDepth = GraphicInterface->RequestRenderTexture("SceneDepth", RenderResolution, DepthFormat, true);
	SceneTranslucentDepth = GraphicInterface->RequestRenderTexture("SceneTranslucentDepth", RenderResolution, DepthFormat, true);

	// vertex normal buffer for saving the "search ray" in RT shadows
	SceneVertexNormal = GraphicInterface->RequestRenderTexture("SceneVertexNormal", RenderResolution, NormalFormat, true);
	SceneTranslucentVertexNormal = GraphicInterface->RequestRenderTexture("SceneTranslucentVertexNormal", RenderResolution, NormalFormat, true);

	// post process buffer, use the same format as scene result
	PostProcessRT = GraphicInterface->RequestRenderTexture("PostProcessRT", RenderResolution, SceneResultFormat, true, true);
	PreviousSceneResult = GraphicInterface->RequestRenderTexture("PreviousResultRT", RenderResolution, SceneResultFormat, true);

	// motion vector buffer
	MotionVectorRT = GraphicInterface->RequestRenderTexture("MotionVectorRT", RenderResolution, MotionFormat, true);
	PrevMotionVectorRT = GraphicInterface->RequestRenderTexture("PrevMotionVectorRT", RenderResolution, MotionFormat, true);

	// rt shadows buffer
	if (GraphicInterface->IsRayTracingEnabled())
	{
		int32_t ShadowQuality = std::clamp(ConfigInterface->RenderingSetting().RTDirectionalShadowQuality, 0, 2);
		RTShadowExtent.width = RenderResolution.width >> ShadowQuality;
		RTShadowExtent.height = RenderResolution.height >> ShadowQuality;
		RTShadowResult = GraphicInterface->RequestRenderTexture("RTShadowResult", RTShadowExtent, RTShadowFormat, true, true);
		RTTranslucentShadow = GraphicInterface->RequestRenderTexture("RTTranslucentShadow", RTShadowExtent, RTShadowFormat, true, true);
	}

	// collect image views for creaing one frame buffer
	std::vector<VkImageView> Views;
	Views.push_back(SceneDiffuse->GetImageView());
	Views.push_back(SceneNormal->GetImageView());
	Views.push_back(SceneMaterial->GetImageView());
	Views.push_back(SceneResult->GetImageView());
	Views.push_back(SceneMip->GetImageView());
	Views.push_back(SceneVertexNormal->GetImageView());
	Views.push_back(SceneDepth->GetImageView());

	// depth frame buffer
	if (bEnableDepthPrePass)
	{
		DepthPassObj.FrameBuffer = GraphicInterface->CreateFrameBuffer(SceneDepth->GetImageView(), DepthPassObj.RenderPass, RenderResolution);
	}

	// create frame buffer, some of them can be shared, especially when the output target is the same
	BasePassObj.FrameBuffer = GraphicInterface->CreateFrameBuffer(Views, BasePassObj.RenderPass, RenderResolution);

	// sky pass need depth
	Views = { SceneResult->GetImageView() , SceneDepth->GetImageView() };
	SkyboxPassObj.FrameBuffer = GraphicInterface->CreateFrameBuffer(Views, SkyboxPassObj.RenderPass, RenderResolution);

	// translucent pass can share the same frame buffer as skybox pass
	TranslucentPassObj.FrameBuffer = SkyboxPassObj.FrameBuffer;

	// post process pass, use two buffer and blit each other
	PostProcessPassObj[0].FrameBuffer = GraphicInterface->CreateFrameBuffer(PostProcessRT->GetImageView(), PostProcessPassObj[0].RenderPass, RenderResolution);
	PostProcessPassObj[1].FrameBuffer = GraphicInterface->CreateFrameBuffer(SceneResult->GetImageView(), PostProcessPassObj[1].RenderPass, RenderResolution);

	// motion pass framebuffer
	MotionCameraPassObj.FrameBuffer = GraphicInterface->CreateFrameBuffer(MotionVectorRT->GetImageView(), MotionCameraPassObj.RenderPass, RenderResolution);
	Views = { MotionVectorRT->GetImageView(), SceneDepth->GetImageView() };
	MotionOpaquePassObj.FrameBuffer = GraphicInterface->CreateFrameBuffer(Views, MotionOpaquePassObj.RenderPass, RenderResolution);

	Views = { MotionVectorRT->GetImageView(), SceneTranslucentVertexNormal->GetImageView(), SceneTranslucentDepth->GetImageView() };
	MotionTranslucentPassObj.FrameBuffer = GraphicInterface->CreateFrameBuffer(Views, MotionTranslucentPassObj.RenderPass, RenderResolution);
}

void UHDeferredShadingRenderer::RelaseRenderingBuffers()
{
	GraphicInterface->RequestReleaseRT(SceneDiffuse);
	GraphicInterface->RequestReleaseRT(SceneNormal);
	GraphicInterface->RequestReleaseRT(SceneMaterial);
	GraphicInterface->RequestReleaseRT(SceneResult);
	GraphicInterface->RequestReleaseRT(SceneMip);
	GraphicInterface->RequestReleaseRT(SceneDepth);
	GraphicInterface->RequestReleaseRT(SceneTranslucentDepth);
	GraphicInterface->RequestReleaseRT(SceneVertexNormal);
	GraphicInterface->RequestReleaseRT(SceneTranslucentVertexNormal);
	GraphicInterface->RequestReleaseRT(PostProcessRT);
	GraphicInterface->RequestReleaseRT(PreviousSceneResult);
	GraphicInterface->RequestReleaseRT(MotionVectorRT);
	GraphicInterface->RequestReleaseRT(PrevMotionVectorRT);

	if (GraphicInterface->IsRayTracingEnabled())
	{
		GraphicInterface->RequestReleaseRT(RTShadowResult);
		GraphicInterface->RequestReleaseRT(RTTranslucentShadow);
	}
}

void UHDeferredShadingRenderer::CreateRenderPasses()
{
	std::vector<VkFormat> GBufferFormats;
	GBufferFormats.push_back(DiffuseFormat);
	GBufferFormats.push_back(NormalFormat);
	GBufferFormats.push_back(SpecularFormat);
	GBufferFormats.push_back(SceneResultFormat);
	GBufferFormats.push_back(SceneMipFormat);
	GBufferFormats.push_back(NormalFormat);

	// depth prepass
	if (bEnableDepthPrePass)
	{
		DepthPassObj.RenderPass = GraphicInterface->CreateRenderPass(UHTransitionInfo(), DepthFormat);
	}

	// create render pass based on output RT, render pass can be shared sometimes
	BasePassObj.RenderPass = GraphicInterface->CreateRenderPass(GBufferFormats, UHTransitionInfo(bEnableDepthPrePass ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR)
		, DepthFormat);

	// sky need depth
	SkyboxPassObj.RenderPass = GraphicInterface->CreateRenderPass(SceneResultFormat
		, UHTransitionInfo(VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		, DepthFormat);

	// translucent pass can share the same render pass obj as skybox pass
	TranslucentPassObj.RenderPass = SkyboxPassObj.RenderPass;

	// post processing render pass
	PostProcessPassObj[0].RenderPass = GraphicInterface->CreateRenderPass(SceneResultFormat
		, UHTransitionInfo(VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
	PostProcessPassObj[1].RenderPass = PostProcessPassObj[0].RenderPass;

	// motion pass, opaque and translucent can share the same RenderPass
	MotionCameraPassObj.RenderPass = GraphicInterface->CreateRenderPass(MotionFormat, UHTransitionInfo());
	MotionOpaquePassObj.RenderPass = GraphicInterface->CreateRenderPass(MotionFormat
		, UHTransitionInfo(VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		, DepthFormat);

	std::vector<VkFormat> TranslucentMotionFormats = { MotionFormat , NormalFormat };
	MotionTranslucentPassObj.RenderPass = GraphicInterface->CreateRenderPass(TranslucentMotionFormats
		, UHTransitionInfo(VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		, DepthFormat);
}

void UHDeferredShadingRenderer::ReleaseRenderPassObjects(bool bFrameBufferOnly)
{
	VkDevice LogicalDevice = GraphicInterface->GetLogicalDevice();

	if (!bFrameBufferOnly)
	{
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
	else
	{
		if (bEnableDepthPrePass)
		{
			DepthPassObj.ReleaseFrameBuffer(LogicalDevice);
		}

		BasePassObj.ReleaseFrameBuffer(LogicalDevice);
		SkyboxPassObj.ReleaseFrameBuffer(LogicalDevice);
		MotionCameraPassObj.ReleaseFrameBuffer(LogicalDevice);
		MotionOpaquePassObj.ReleaseFrameBuffer(LogicalDevice);
		MotionTranslucentPassObj.ReleaseFrameBuffer(LogicalDevice);

		for (UHRenderPassObject& P : PostProcessPassObj)
		{
			P.ReleaseFrameBuffer(LogicalDevice);
		}
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
		SystemConstantsGPU[Idx] = GraphicInterface->RequestRenderBuffer<UHSystemConstants>();
		SystemConstantsGPU[Idx]->CreateBuffer(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

		ObjectConstantsGPU[Idx] = GraphicInterface->RequestRenderBuffer<UHObjectConstants>();
		ObjectConstantsGPU[Idx]->CreateBuffer(CurrentScene->GetAllRendererCount(), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		ObjectConstantsCPU.resize(CurrentScene->GetAllRendererCount());

		DirectionalLightBuffer[Idx] = GraphicInterface->RequestRenderBuffer<UHDirectionalLightConstants>();
		DirectionalLightBuffer[Idx]->CreateBuffer(CurrentScene->GetDirLightCount(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		DirLightConstantsCPU.resize(CurrentScene->GetDirLightCount());
	}
}

void UHDeferredShadingRenderer::CreateThreadObjects()
{
#if WITH_DEBUG
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
	RenderThread = std::make_unique<UHThread>();
	RenderThread->BeginThread(std::thread(&UHDeferredShadingRenderer::RenderThreadLoop, this), GRenderThreadAffinity);
	WorkerThreads.resize(NumWorkerThreads);

	for (int32_t Idx = 0; Idx < NumWorkerThreads; Idx++)
	{
		WorkerThreads[Idx] = std::make_unique<UHThread>();
		WorkerThreads[Idx]->BeginThread(std::thread(&UHDeferredShadingRenderer::WorkerThreadLoop, this, Idx), GWorkerThreadAffinity + Idx);
	}
}

void UHDeferredShadingRenderer::ReleaseDataBuffers()
{
	for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		UH_SAFE_RELEASE(SystemConstantsGPU[Idx]);
		SystemConstantsGPU[Idx].reset();

		UH_SAFE_RELEASE(ObjectConstantsGPU[Idx]);
		ObjectConstantsGPU[Idx].reset();
	}
}

void UHDeferredShadingRenderer::ResizeRTBuffers()
{
	if (GraphicInterface->IsRayTracingEnabled())
	{
		GraphicInterface->WaitGPU();
		GraphicInterface->RequestReleaseRT(RTShadowResult);
		GraphicInterface->RequestReleaseRT(RTTranslucentShadow);

		int32_t ShadowQuality = std::clamp(ConfigInterface->RenderingSetting().RTDirectionalShadowQuality, 0, 2);
		RTShadowExtent.width = RenderResolution.width >> ShadowQuality;
		RTShadowExtent.height = RenderResolution.height >> ShadowQuality;
		RTShadowResult = GraphicInterface->RequestRenderTexture("RTShadowResult", RTShadowExtent, RTShadowFormat, true, true);
		RTTranslucentShadow = GraphicInterface->RequestRenderTexture("RTTranslucentShadow", RTShadowExtent, RTShadowFormat, true, true);

		// need to rewrite descriptors after resize
		UpdateDescriptors();
	}
}

#if WITH_DEBUG
template <typename T>
void SafeReleaseShader(std::unordered_map<int32_t, T>& InMap, const int32_t InIndex)
{
	if (InMap.find(InIndex) != InMap.end())
	{
		InMap[InIndex].Release();
	}
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

	const bool bIsOpaque = Mat->GetBlendMode() < UHBlendMode::TranditionalAlpha;
	const std::vector<VkDescriptorSetLayout> BindlessLayouts = { TextureTable.GetDescriptorSetLayout(), SamplerTable.GetDescriptorSetLayout() };

	for (UHObject* RendererObj : Mat->GetReferenceObjects())
	{
		if (UHMeshRendererComponent* Renderer = static_cast<UHMeshRendererComponent*>(RendererObj))
		{
			int32_t RendererBufferIndex = Renderer->GetBufferDataIndex();

			if (CompileFlag == FullCompileTemporary || CompileFlag == FullCompileResave)
			{
				// release shaders if it's re-compiling
				if (bEnableDepthPrePass)
				{
					SafeReleaseShader(DepthPassShaders, RendererBufferIndex);
				}

				SafeReleaseShader(BasePassShaders, RendererBufferIndex);
				SafeReleaseShader(MotionOpaqueShaders, RendererBufferIndex);
				SafeReleaseShader(MotionTranslucentShaders, RendererBufferIndex);
				SafeReleaseShader(TranslucentPassShaders, RendererBufferIndex);
			}
			else if (CompileFlag == BindOnly)
			{
				// bind only
				if (bIsOpaque)
				{
					if (bEnableDepthPrePass)
					{
						DepthPassShaders[RendererBufferIndex].BindParameters(SystemConstantsGPU, ObjectConstantsGPU, Renderer);
					}

					BasePassShaders[RendererBufferIndex].BindParameters(SystemConstantsGPU, ObjectConstantsGPU
						, Renderer);

					MotionOpaqueShaders[RendererBufferIndex].BindParameters(SystemConstantsGPU, ObjectConstantsGPU, Renderer);
				}
				else
				{
					MotionTranslucentShaders[RendererBufferIndex].BindParameters(SystemConstantsGPU, ObjectConstantsGPU, Renderer);
					TranslucentPassShaders[RendererBufferIndex].BindParameters(SystemConstantsGPU, ObjectConstantsGPU, DirectionalLightBuffer
						, RTTranslucentShadow, LinearClampedSampler, Renderer);
				}
			}
			else if (CompileFlag == StateChangedOnly)
			{
				// re-create state only
				if (bIsOpaque)
				{
					if (bEnableDepthPrePass)
					{
						DepthPassShaders[RendererBufferIndex].RecreateMaterialState();
					}
					BasePassShaders[RendererBufferIndex].RecreateMaterialState();
					MotionOpaqueShaders[RendererBufferIndex].RecreateMaterialState();
				}
				else
				{
					MotionTranslucentShaders[RendererBufferIndex].RecreateMaterialState();
					TranslucentPassShaders[RendererBufferIndex].RecreateMaterialState();
				}
			}

			// re-assign renderer and erase old shader instances
			if (bNeedReassignRendererGroup)
			{
				CurrentScene->ReassignRenderer(Renderer);
				DepthPassShaders.erase(RendererBufferIndex);
				BasePassShaders.erase(RendererBufferIndex);
				MotionOpaqueShaders.erase(RendererBufferIndex);
				MotionTranslucentShaders.erase(RendererBufferIndex);
				TranslucentPassShaders.erase(RendererBufferIndex);
			}

			Renderer->SetRayTracingDirties(true);
		}
	}

	// re-create shader and bind parameters
	if (CompileFlag == FullCompileTemporary || CompileFlag == FullCompileResave)
	{
		for (UHObject* RendererObj : Mat->GetReferenceObjects())
		{
			if (UHMeshRendererComponent* Renderer = static_cast<UHMeshRendererComponent*>(RendererObj))
			{
				int32_t RendererBufferIndex = Renderer->GetBufferDataIndex();

				if (bIsOpaque)
				{
					if (bEnableDepthPrePass)
					{
						DepthPassShaders[RendererBufferIndex] = UHDepthPassShader(GraphicInterface, "DepthPassShader", DepthPassObj.RenderPass, Mat, BindlessLayouts);
						DepthPassShaders[RendererBufferIndex].BindParameters(SystemConstantsGPU, ObjectConstantsGPU, Renderer);
					}

					BasePassShaders[RendererBufferIndex] = UHBasePassShader(GraphicInterface, "BasePassShader", BasePassObj.RenderPass, Mat, bEnableDepthPrePass, BindlessLayouts);
					BasePassShaders[RendererBufferIndex].BindParameters(SystemConstantsGPU, ObjectConstantsGPU
						, Renderer);

					MotionOpaqueShaders[RendererBufferIndex] = UHMotionObjectPassShader(GraphicInterface, "MotionObjectShader", MotionOpaquePassObj.RenderPass, Mat, bEnableDepthPrePass, BindlessLayouts);
					MotionOpaqueShaders[RendererBufferIndex].BindParameters(SystemConstantsGPU, ObjectConstantsGPU, Renderer);
				}
				else
				{
					MotionTranslucentShaders[RendererBufferIndex] = UHMotionObjectPassShader(GraphicInterface, "MotionObjectShader", MotionTranslucentPassObj.RenderPass, Mat, bEnableDepthPrePass, BindlessLayouts);
					MotionTranslucentShaders[RendererBufferIndex].BindParameters(SystemConstantsGPU, ObjectConstantsGPU, Renderer);

					TranslucentPassShaders[RendererBufferIndex]
						= UHTranslucentPassShader(GraphicInterface, "TranslucentPassShader", TranslucentPassObj.RenderPass, Mat, BindlessLayouts);
					TranslucentPassShaders[RendererBufferIndex].BindParameters(SystemConstantsGPU, ObjectConstantsGPU, DirectionalLightBuffer
						, RTTranslucentShadow, LinearClampedSampler, Renderer);
				}

				Renderer->SetRayTracingDirties(true);
			}
		}
	}

	// update hit group shader as well
	if (GraphicInterface->IsRayTracingEnabled() && CompileFlag != BindOnly && CompileFlag != StateChangedOnly)
	{
		RTShadowShader.Release();
		RTDefaultHitGroupShader.UpdateHitShader(GraphicInterface, Mat);

		const std::vector<VkDescriptorSetLayout> Layouts = { TextureTable.GetDescriptorSetLayout()
			, SamplerTable.GetDescriptorSetLayout()
			, RTVertexTable.GetDescriptorSetLayout()
			, RTIndicesTable.GetDescriptorSetLayout()
			, RTIndicesTypeTable.GetDescriptorSetLayout()
			, RTMaterialDataTable.GetDescriptorSetLayout() };

		RTShadowShader = UHRTShadowShader(GraphicInterface, "RTShadowShader", RTDefaultHitGroupShader.GetClosestShaders(), RTDefaultHitGroupShader.GetAnyHitShaders()
			, Layouts);
	}

	Mat->SetCompileFlag(UpToDate);

	// mark render dirties for re-uploading constant and updating TLAS
	Mat->SetRenderDirties(true);

	UpdateDescriptors();
}
#endif