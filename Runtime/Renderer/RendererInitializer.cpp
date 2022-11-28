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
	, MainCommandPool(VK_NULL_HANDLE)
	, CurrentFrame(0)
	, bIsThisFrameRenderedShared(true)
	, bIsRenderThreadDoneShared(false)
	, bIsResetNeededShared(false)
	, CurrentScene(nullptr)
	, SystemConstantsCPU(UHSystemConstants())
	, SceneDiffuse(nullptr)
	, SceneNormal(nullptr)
	, SceneMaterial(nullptr)
	, SceneResult(nullptr)
	, SceneMip(nullptr)
	, SceneDepth(nullptr)
	, PointClampedSampler(nullptr)
	, LinearClampedSampler(nullptr)
	, PostProcessRT(nullptr)
	, PreviousSceneResult(nullptr)
	, PostProcessResultIdx(0)
	, bIsTemporalReset(true)
	, MotionVectorRT(nullptr)
	, PrevMotionVectorRT(nullptr)
	, RTShadowResult(nullptr)
	, RTInstanceCount(0)
	, IndicesTypeBuffer(nullptr)
#if WITH_DEBUG
	, DebugViewIndex(0)
	, RenderThreadTime(0)
	, DrawCalls(0)
#endif
{
	// init static array pointers
	for (size_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		MainCommandBuffers[Idx] = VK_NULL_HANDLE;
		SwapChainAvailableSemaphores[Idx] = VK_NULL_HANDLE;
		RenderFinishedSemaphores[Idx] = VK_NULL_HANDLE;
		MainFences[Idx] = VK_NULL_HANDLE;
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
	SceneResultFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

	// despite NVIDIA suggests 24-bit depth, I need 32-bit depth since it's needed to be sampled in shader
	// and Vulkan currently supports sampling 16/32 bits depth only
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

	// shared sampler requests
	UHSamplerInfo PointClampedInfo(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	PointClampedInfo.MaxAnisotropy = 1;
	PointClampedSampler = GraphicInterface->RequestTextureSampler(PointClampedInfo);

	UHSamplerInfo LinearClampedInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	LinearClampedInfo.MaxAnisotropy = 1;
	LinearClampedSampler = GraphicInterface->RequestTextureSampler(LinearClampedInfo);

	bool bIsRendererSuccess = CreateMainCommandPoolAndBuffer() && CreateFences();
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

	#if WITH_DEBUG
		for (int32_t Idx = 0; Idx < UHRenderPassMax; Idx++)
		{
			GPUTimeQueries[Idx] = GraphicInterface->RequestGPUQuery(2, VK_QUERY_TYPE_TIMESTAMP);
		}
	#endif

		// init render thread, it will wait at the beginning
		RenderThread = std::thread(&UHDeferredShadingRenderer::RenderThreadLoop, this);
	}

	return bIsRendererSuccess;
}

void UHDeferredShadingRenderer::Release()
{
	VkDevice LogicalDevice = GraphicInterface->GetLogicalDevice();

	// wait device to finish before release
	GraphicInterface->WaitGPU();

	// end render thread
	if (RenderThread.joinable())
	{
		bIsThisFrameRenderedShared = false;
		bIsRenderThreadDoneShared = true;
		WaitRenderThread.notify_one();
		RenderThread.join();
	}

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
		vkDestroySemaphore(LogicalDevice, SwapChainAvailableSemaphores[Idx], nullptr);
		vkDestroySemaphore(LogicalDevice, RenderFinishedSemaphores[Idx], nullptr);
		vkDestroyFence(LogicalDevice, MainFences[Idx], nullptr);

		UH_SAFE_RELEASE(SystemConstantsGPU[Idx]);
		SystemConstantsGPU[Idx].reset();

		UH_SAFE_RELEASE(ObjectConstantsGPU[Idx]);
		ObjectConstantsGPU[Idx].reset();

		UH_SAFE_RELEASE(MaterialConstantsGPU[Idx]);
		MaterialConstantsGPU[Idx].reset();

		UH_SAFE_RELEASE(DirectionalLightBuffer[Idx]);
		DirectionalLightBuffer[Idx].reset();

		if (GEnableRayTracing)
		{
			UH_SAFE_RELEASE(TopLevelAS[Idx]);
			TopLevelAS[Idx].reset();
		}
	}

	UH_SAFE_RELEASE(IndicesTypeBuffer);
	IndicesTypeBuffer.reset();

	vkDestroyCommandPool(LogicalDevice, MainCommandPool, nullptr);
}

void UHDeferredShadingRenderer::PrepareMeshes()
{
	// create mesh buffer for all default lit renderers
	std::vector<UHMeshRendererComponent*> Renderers = CurrentScene->GetAllRenderers();

	if (Renderers.size() == 0)
	{
		return;
	}

	// needs the cmd buffer
	VkCommandBuffer CreationCmd = GraphicInterface->BeginOneTimeCmd();

	for (UHMeshRendererComponent* Renderer : Renderers)
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
	for (UHMeshRendererComponent* Renderer : Renderers)
	{
		Renderer->GetMesh()->ReleaseCPUMeshData();
	}

	// release scratch data of AS after creation
	for (UHMeshRendererComponent* Renderer : Renderers)
	{
		if (Renderer->GetMesh()->GetBottomLevelAS())
		{
			Renderer->GetMesh()->GetBottomLevelAS()->ReleaseScratch();
		}
	}
}

void UHDeferredShadingRenderer::PrepareTextures()
{
	// instead of uploading to GPU right after import, I choose to upload textures if they're really in use
	// this makes workflow complicated but good for GPU memory

	// Step1: uploading all textures which are really using for rendering
	VkCommandBuffer CreationCmd = GraphicInterface->BeginOneTimeCmd();
	UHGraphicBuilder GraphBuilder(GraphicInterface, CreationCmd);

	for (UHMeshRendererComponent* Renderer : CurrentScene->GetAllRenderers())
	{
		UHMaterial* Mat = Renderer->GetMaterial();
		for (int32_t Idx = 0; Idx < UHMaterialTextureType::TextureTypeMax; Idx++)
		{
			UHTexture* Tex = Mat->GetTex(static_cast<UHMaterialTextureType>(Idx));
			if (Tex)
			{
				Tex->UploadToGPU(GraphicInterface, CreationCmd, GraphBuilder);
			}
		}
	}
	
	// Step 2: Generate mip maps for all uploaded textures
	for (UHMeshRendererComponent* Renderer : CurrentScene->GetAllRenderers())
	{
		UHMaterial* Mat = Renderer->GetMaterial();
		for (int32_t Idx = 0; Idx < UHMaterialTextureType::TextureTypeMax; Idx++)
		{
			UHTexture* Tex = Mat->GetTex(static_cast<UHMaterialTextureType>(Idx));
			if (Tex)
			{
				Tex->GenerateMipMaps(GraphicInterface, CreationCmd, GraphBuilder);
			}
		}
	}

	// Step3: Build all cubemaps in use
	for (UHMeshRendererComponent* Renderer : CurrentScene->GetAllRenderers())
	{
		UHMaterial* Mat = Renderer->GetMaterial();
		if (Mat && Mat->GetTex(UHMaterialTextureType::SkyCube))
		{
			UHTextureCube* Cube = static_cast<UHTextureCube*>(Mat->GetTex(UHMaterialTextureType::SkyCube));
			Cube->Build(GraphicInterface, CreationCmd, GraphBuilder);
		}
	}

	if (UHMeshRendererComponent* SkyRenderer = CurrentScene->GetSkyboxRenderer())
	{
		if (SkyRenderer->GetMaterial() && SkyRenderer->GetMaterial()->GetTex(UHMaterialTextureType::SkyCube))
		{
			UHTextureCube* Cube = static_cast<UHTextureCube*>(SkyRenderer->GetMaterial()->GetTex(UHMaterialTextureType::SkyCube));
			Cube->Build(GraphicInterface, CreationCmd, GraphBuilder);
		}
	}

	GraphicInterface->EndOneTimeCmd(CreationCmd);
}

void UHDeferredShadingRenderer::PrepareRenderingShaders()
{
	// base pass shader, opaque only
	for (UHMeshRendererComponent* Renderer : CurrentScene->GetOpaqueRenderers())
	{
		UHMaterial* Mat = Renderer->GetMaterial();
		if (Mat->IsSkybox())
		{
			continue;
		}

		if (Mat && BasePassShaders.find(Renderer->GetBufferDataIndex()) == BasePassShaders.end())
		{
			BasePassShaders[Renderer->GetBufferDataIndex()] = UHBasePassShader(GraphicInterface, "BasePassShader", BasePassObj.RenderPass, Mat);
		}
	}

	// light pass shader
	LightPassShader = UHLightPassShader(GraphicInterface, "LightPassShader", LightPassObj.RenderPass, RTInstanceCount);

	// sky pass shader
	UHMeshRendererComponent* SkyRenderer = CurrentScene->GetSkyboxRenderer();
	SkyPassShader = UHSkyPassShader(GraphicInterface, "SkyPassShader", SkyboxPassObj.RenderPass);

	// motion pass shader
	MotionCameraShader = UHMotionCameraPassShader(GraphicInterface, "MotionCameraPassShader", MotionCameraPassObj.RenderPass);
	for (UHMeshRendererComponent* Renderer : CurrentScene->GetAllRenderers())
	{
		UHMaterial* Mat = Renderer->GetMaterial();
		if (Mat->IsSkybox())
		{
			continue;
		}

		if (Mat && MotionObjectShaders.find(Renderer->GetBufferDataIndex()) == MotionObjectShaders.end())
		{
			MotionObjectShaders[Renderer->GetBufferDataIndex()] = UHMotionObjectPassShader(GraphicInterface, "MotionObjectShader", MotionObjectPassObj.RenderPass, Mat);
		}
	}

	// post processing shaders
	TemporalAAShader = UHTemporalAAShader(GraphicInterface, "TemporalAAShader", PostProcessPassObj[0].RenderPass);
	ToneMapShader = UHToneMappingShader(GraphicInterface, "ToneMapShader", PostProcessPassObj[0].RenderPass);

	// RT shaders
	if (GEnableRayTracing)
	{
		RTVertexTable = UHRTVertexTable(GraphicInterface, "RTVertexTable", RTInstanceCount);
		RTIndicesTable = UHRTIndicesTable(GraphicInterface, "RTIndicesTable", RTInstanceCount);

		// buffer for storing index type, used in hit group shader
		RTIndicesTypeTable = UHRTIndicesTypeTable(GraphicInterface, "RTIndicesTypeTable");
		IndicesTypeBuffer = GraphicInterface->RequestRenderBuffer<int32_t>();
		IndicesTypeBuffer->CreaetBuffer(RTInstanceCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

		RTTextureTable = UHRTTextureTable(GraphicInterface, "RTTextureTable", static_cast<uint32_t>(AssetManagerInterface->GetReferencedTexture2Ds().size()));
		RTSamplerTable = UHRTSamplerTable(GraphicInterface, "RTSamplerTable", static_cast<uint32_t>(GraphicInterface->GetSamplers().size()));
		RTDefaultHitGroupShader = UHRTDefaultHitGroupShader(GraphicInterface, "RTDefaultHitGroupShader");

		// also send texture & VB/IB layout to RT shadow shader
		std::vector<VkDescriptorSetLayout> Layouts = { RTTextureTable.GetDescriptorSetLayout()
			, RTSamplerTable.GetDescriptorSetLayout()
			, RTVertexTable.GetDescriptorSetLayout()
			, RTIndicesTable.GetDescriptorSetLayout()
			, RTIndicesTypeTable.GetDescriptorSetLayout() };

		RTShadowShader = UHRTShadowShader(GraphicInterface, "RTShadowShader", RTDefaultHitGroupShader.GetClosestShader(), RTDefaultHitGroupShader.GetAnyHitShader()
			, Layouts);
	}

#if WITH_DEBUG
	DebugViewShader = UHDebugViewShader(GraphicInterface, "DebugViewShader", PostProcessPassObj[0].RenderPass);
#endif
}

bool UHDeferredShadingRenderer::CreateMainCommandPoolAndBuffer()
{
	VkDevice LogicalDevice = GraphicInterface->GetLogicalDevice();
	UHQueueFamily QueueFamily = GraphicInterface->GetQueueFamily();

	VkCommandPoolCreateInfo PoolInfo{};
	PoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

	// I'd like to reset and record every frame
	PoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	PoolInfo.queueFamilyIndex = QueueFamily.GraphicsFamily.value();

	if (vkCreateCommandPool(LogicalDevice, &PoolInfo, nullptr, &MainCommandPool) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to create command pool!\n");
		return false;
	}

	// now create command buffer
	VkCommandBufferAllocateInfo AllocInfo{};
	AllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	AllocInfo.commandPool = MainCommandPool;
	AllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	AllocInfo.commandBufferCount = GMaxFrameInFlight;

	if (vkAllocateCommandBuffers(LogicalDevice, &AllocInfo, MainCommandBuffers.data()) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to allocate command buffers!\n");
		return false;
	}

	return true;
}

void UHDeferredShadingRenderer::UpdateDescriptors()
{
	VkDevice LogicalDevice = GraphicInterface->GetLogicalDevice();

	// ------------------------------------------------ Base pass descriptor update
	// after creation, update descriptor info for all renderers
	for(UHMeshRendererComponent* Renderer : CurrentScene->GetOpaqueRenderers())
	{
		if (BasePassShaders.find(Renderer->GetBufferDataIndex()) == BasePassShaders.end())
		{
			// unlikely to happen, but printing a message for debug
			UHE_LOG(L"[UpdateDescriptors] Can't find base pass shader for renderer\n");
			continue;
		}

		UHMaterial* Mat = Renderer->GetMaterial();
		UHBasePassShader& BaseShader = BasePassShaders[Renderer->GetBufferDataIndex()];
		BaseShader.BindConstant(SystemConstantsGPU, 0);
		BaseShader.BindConstant(ObjectConstantsGPU, 1, Renderer->GetBufferDataIndex());
		BaseShader.BindStorage(MaterialConstantsGPU, 2, Renderer->GetMaterial()->GetBufferDataIndex());

		// write textures/samplers when they are available
		for (int32_t Tdx = 0; Tdx < UHMaterialTextureType::TextureTypeMax; Tdx++)
		{
			UHTexture* Tex = Mat->GetTex(static_cast<UHMaterialTextureType>(Tdx));
			if (Tex)
			{
				BaseShader.BindImage(Tex, UHConstantTypes::ConstantTypeMax + Tdx * 2);
			}

			UHSampler* Sampler = Mat->GetSampler(static_cast<UHMaterialTextureType>(Tdx));
			if (Sampler)
			{
				BaseShader.BindSampler(Sampler, UHConstantTypes::ConstantTypeMax + 1 + Tdx * 2);
			}
		}

		UHMesh* Mesh = Renderer->GetMesh();
		BaseShader.BindStorage(Mesh->GetUV0Buffer(), 19, 0, true);
		BaseShader.BindStorage(Mesh->GetNormalBuffer(), 20, 0, true);
		BaseShader.BindStorage(Mesh->GetTangentBuffer(), 21, 0, true);
	}

	// ------------------------------------------------ Lighting pass descriptor update
	LightPassShader.BindConstant(SystemConstantsGPU, 0);
	LightPassShader.BindStorage(DirectionalLightBuffer, 1, 0, true);

	std::vector<UHTexture*> Textures = { SceneDiffuse, SceneNormal, SceneMaterial, SceneDepth, SceneMip };
	LightPassShader.BindImage(Textures, 2);
	LightPassShader.BindSampler(LinearClampedSampler, 3);

	if (GEnableRayTracing && RTInstanceCount > 0)
	{
		LightPassShader.BindImage(RTShadowResult, 4);
	}

	// ------------------------------------------------ sky pass descriptor update
	UHMeshRendererComponent* SkyRenderer = CurrentScene->GetSkyboxRenderer();
	if (SkyRenderer)
	{
		SkyPassShader.BindConstant(SystemConstantsGPU, 0);
		SkyPassShader.BindConstant(ObjectConstantsGPU, 1, SkyRenderer->GetBufferDataIndex());
		SkyPassShader.BindImage(CurrentScene->GetSkyboxRenderer()->GetMaterial()->GetTex(UHMaterialTextureType::SkyCube), 2);
		SkyPassShader.BindSampler(CurrentScene->GetSkyboxRenderer()->GetMaterial()->GetSampler(UHMaterialTextureType::SkyCube), 3);
	}

	// ------------------------------------------------ motion pass descriptor update
	MotionCameraShader.BindConstant(SystemConstantsGPU, 0);
	MotionCameraShader.BindImage(SceneDepth, 1);
	MotionCameraShader.BindSampler(PointClampedSampler, 2);

	for (UHMeshRendererComponent* Renderer : CurrentScene->GetAllRenderers())
	{
		UHMaterial* Mat = Renderer->GetMaterial();
		UHMesh* Mesh = Renderer->GetMesh();
		if (Mat->IsSkybox())
		{
			continue;
		}

		if (MotionObjectShaders.find(Renderer->GetBufferDataIndex()) == MotionObjectShaders.end())
		{
			// unlikely to happen, but printing a message for debug
			UHE_LOG(L"[UpdateDescriptors] Can't find motion object pass shader for renderer\n");
			continue;
		}

		UHMotionObjectPassShader& MotionObjectShader = MotionObjectShaders[Renderer->GetBufferDataIndex()];
		MotionObjectShader.BindConstant(SystemConstantsGPU, 0);
		MotionObjectShader.BindConstant(ObjectConstantsGPU, 1, Renderer->GetBufferDataIndex());
		MotionObjectShader.BindStorage(MaterialConstantsGPU, 2, Renderer->GetMaterial()->GetBufferDataIndex());

		if (UHTexture* OpacityTex = Renderer->GetMaterial()->GetTex(UHMaterialTextureType::Opacity))
		{
			MotionObjectShader.BindImage(OpacityTex, 3);
		}

		if (UHSampler* OpacitySampler = Renderer->GetMaterial()->GetSampler(UHMaterialTextureType::Opacity))
		{
			MotionObjectShader.BindSampler(OpacitySampler, 4);
		}

		MotionObjectShader.BindStorage(Mesh->GetUV0Buffer(), 5, 0, true);
	}

	// ------------------------------------------------ post process pass descriptor update
	// when binding post processing input, be sure to bind it alternately, the two RT will keep bliting to each other
	// the post process RT binding will be in PostProcessRendering.cpp
	ToneMapShader.BindSampler(LinearClampedSampler, 1);

	TemporalAAShader.BindConstant(SystemConstantsGPU, 0);
	TemporalAAShader.BindImage(PreviousSceneResult, 2);
	TemporalAAShader.BindImage(MotionVectorRT, 3);
	TemporalAAShader.BindImage(PrevMotionVectorRT, 4);
	TemporalAAShader.BindImage(SceneDepth, 5);
	TemporalAAShader.BindSampler(LinearClampedSampler, 6);

	// ------------------------------------------------ ray tracing pass descriptor update
	if (GEnableRayTracing && TopLevelAS[0] && RTInstanceCount > 0)
	{
		// bind VB/IB table
		std::vector<UHMeshRendererComponent*> Renderers = CurrentScene->GetAllRenderers();
		std::vector<UHRenderBuffer<XMFLOAT2>*> UVs;
		std::vector<VkDescriptorBufferInfo> BufferInfos;
		std::vector<int32_t> IndexTypes;

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

		IndicesTypeBuffer->UploadAllData(IndexTypes.data());
		RTIndicesTypeTable.BindStorage(IndicesTypeBuffer.get(), 0, 0, true);

		// bind texture table
		std::vector<UHTexture*> Texes(AssetManagerInterface->GetReferencedTexture2Ds().size());
		for (size_t Idx = 0; Idx < Texes.size(); Idx++)
		{
			Texes[Idx] = AssetManagerInterface->GetReferencedTexture2Ds()[Idx];
		}
		RTTextureTable.BindImage(Texes, 0);

		// bind sampler table
		std::vector<UHSampler*> Samplers = GraphicInterface->GetSamplers();
		RTSamplerTable.BindSampler(Samplers, 0);

		// bind shader parameters
		RTShadowShader.BindConstant(SystemConstantsGPU, 0);
		RTShadowShader.BindRWImage(RTShadowResult, 2);
		RTShadowShader.BindStorage(DirectionalLightBuffer, 3, 0, true);
		RTShadowShader.BindImage(SceneMip, 4);
		RTShadowShader.BindImage(SceneNormal, 5);
		RTShadowShader.BindImage(SceneDepth, 6);
		RTShadowShader.BindSampler(PointClampedSampler, 7);
		RTShadowShader.BindSampler(LinearClampedSampler, 8);
		RTShadowShader.BindStorage(MaterialConstantsGPU, GMaterialSlotInRT, 0, true);
	}

	// ------------------------------------------------ debug view pass descriptor update
#if WITH_DEBUG
	if (GEnableRayTracing && RTInstanceCount > 0)
	{
		DebugViewShader.BindImage(RTShadowResult, 0);
	}
	DebugViewShader.BindSampler(PointClampedSampler, 1);
#endif
}

void UHDeferredShadingRenderer::ReleaseShaders()
{
	for (auto& BaseShader : BasePassShaders)
	{
		BaseShader.second.Release();
	}
	BasePassShaders.clear();

	for (auto& MotionShader : MotionObjectShaders)
	{
		MotionShader.second.Release();
	}
	MotionObjectShaders.clear();

	LightPassShader.Release();
	SkyPassShader.Release();
	MotionCameraShader.Release();
	TemporalAAShader.Release();
	ToneMapShader.Release();

	if (GEnableRayTracing)
	{
		RTDefaultHitGroupShader.Release();
		RTShadowShader.Release();
		RTTextureTable.Release();
		RTSamplerTable.Release();
		RTVertexTable.Release();
		RTIndicesTable.Release();
		RTIndicesTypeTable.Release();
	}

#if WITH_DEBUG
	DebugViewShader.Release();
#endif
}

bool UHDeferredShadingRenderer::CreateFences()
{
	VkDevice LogicalDevice = GraphicInterface->GetLogicalDevice();

	// init vector
	VkSemaphoreCreateInfo SemaphoreInfo{};
	SemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo FenceInfo{};
	FenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

	// so fence won't be blocked on the first frame
	FenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		if (vkCreateSemaphore(LogicalDevice, &SemaphoreInfo, nullptr, &SwapChainAvailableSemaphores[Idx]) != VK_SUCCESS ||
			vkCreateSemaphore(LogicalDevice, &SemaphoreInfo, nullptr, &RenderFinishedSemaphores[Idx]) != VK_SUCCESS ||
			vkCreateFence(LogicalDevice, &FenceInfo, nullptr, &MainFences[Idx]) != VK_SUCCESS)
		{
			UHE_LOG(L"Failed to allocate fences!\n");
			return false;
		}
	}

	return true;
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
	SceneResult = GraphicInterface->RequestRenderTexture("SceneResult", RenderResolution, SceneResultFormat, true);

	// uv grad buffer
	SceneMip = GraphicInterface->RequestRenderTexture("SceneMip", RenderResolution, SceneMipFormat, true);

	// depth buffer
	SceneDepth = GraphicInterface->RequestRenderTexture("SceneDepth", RenderResolution, DepthFormat, true);

	// post process buffer, use the same format as scene result
	PostProcessRT = GraphicInterface->RequestRenderTexture("PostProcessRT", RenderResolution, SceneResultFormat, true);
	PreviousSceneResult = GraphicInterface->RequestRenderTexture("PreviousResultRT", RenderResolution, SceneResultFormat, true);

	// motion vector buffer
	MotionVectorRT = GraphicInterface->RequestRenderTexture("MotionVectorRT", RenderResolution, MotionFormat, true);
	PrevMotionVectorRT = GraphicInterface->RequestRenderTexture("PrevMotionVectorRT", RenderResolution, MotionFormat, true);

	// rt shadows buffer
	if (GEnableRayTracing)
	{
		int32_t ShadowQuality = std::clamp(ConfigInterface->RenderingSetting().RTDirectionalShadowQuality, 0, 2);
		RTShadowExtent.width = RenderResolution.width >> ShadowQuality;
		RTShadowExtent.height = RenderResolution.height >> ShadowQuality;
		RTShadowResult = GraphicInterface->RequestRenderTexture("RTShadowResult", RTShadowExtent, RTShadowFormat, true, true);
	}

	// collect image views for creaing one frame buffer
	std::vector<VkImageView> Views;
	Views.push_back(SceneDiffuse->GetImageView());
	Views.push_back(SceneNormal->GetImageView());
	Views.push_back(SceneMaterial->GetImageView());
	Views.push_back(SceneResult->GetImageView());
	Views.push_back(SceneMip->GetImageView());
	Views.push_back(SceneDepth->GetImageView());

	// create frame buffer, some of them can be shared, especially when the output target is the same
	BasePassObj.FrameBuffer = GraphicInterface->CreateFrameBuffer(Views, BasePassObj.RenderPass, RenderResolution);
	LightPassObj.FrameBuffer = GraphicInterface->CreateFrameBuffer(SceneResult->GetImageView(), LightPassObj.RenderPass, RenderResolution);

	// sky pass need depth
	Views = { SceneResult->GetImageView() , SceneDepth->GetImageView() };
	SkyboxPassObj.FrameBuffer = GraphicInterface->CreateFrameBuffer(Views, SkyboxPassObj.RenderPass, RenderResolution);

	// post process pass, use two buffer and blit each other
	PostProcessPassObj[0].FrameBuffer = GraphicInterface->CreateFrameBuffer(PostProcessRT->GetImageView(), PostProcessPassObj[0].RenderPass, RenderResolution);
	PostProcessPassObj[1].FrameBuffer = GraphicInterface->CreateFrameBuffer(SceneResult->GetImageView(), PostProcessPassObj[1].RenderPass, RenderResolution);

	// motion pass framebuffer
	MotionCameraPassObj.FrameBuffer = GraphicInterface->CreateFrameBuffer(MotionVectorRT->GetImageView(), MotionCameraPassObj.RenderPass, RenderResolution);
	Views = { MotionVectorRT->GetImageView() , SceneDepth->GetImageView() };
	MotionObjectPassObj.FrameBuffer = GraphicInterface->CreateFrameBuffer(Views, MotionObjectPassObj.RenderPass, RenderResolution);
}

void UHDeferredShadingRenderer::RelaseRenderingBuffers()
{
	GraphicInterface->RequestReleaseRT(SceneDiffuse);
	GraphicInterface->RequestReleaseRT(SceneNormal);
	GraphicInterface->RequestReleaseRT(SceneMaterial);
	GraphicInterface->RequestReleaseRT(SceneResult);
	GraphicInterface->RequestReleaseRT(SceneMip);
	GraphicInterface->RequestReleaseRT(SceneDepth);
	GraphicInterface->RequestReleaseRT(PostProcessRT);
	GraphicInterface->RequestReleaseRT(PreviousSceneResult);
	GraphicInterface->RequestReleaseRT(MotionVectorRT);
	GraphicInterface->RequestReleaseRT(PrevMotionVectorRT);

	if (GEnableRayTracing)
	{
		GraphicInterface->RequestReleaseRT(RTShadowResult);
	}
}

void UHDeferredShadingRenderer::CreateRenderPasses()
{
	std::vector<VkFormat> Formats;
	Formats.push_back(DiffuseFormat);
	Formats.push_back(NormalFormat);
	Formats.push_back(SpecularFormat);
	Formats.push_back(SceneResultFormat);
	Formats.push_back(SceneMipFormat);

	// create render pass based on output RT, render pass can be shared sometimes
	BasePassObj.RenderPass = GraphicInterface->CreateRenderPass(Formats, UHTransitionInfo(), DepthFormat);
	LightPassObj.RenderPass = GraphicInterface->CreateRenderPass(SceneResultFormat
		, UHTransitionInfo(VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

	// sky need depth
	SkyboxPassObj.RenderPass = GraphicInterface->CreateRenderPass(SceneResultFormat
		, UHTransitionInfo(VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		, DepthFormat);

	// post processing render pass
	PostProcessPassObj[0].RenderPass = LightPassObj.RenderPass;
	PostProcessPassObj[1].RenderPass = LightPassObj.RenderPass;

	// motion pass
	MotionCameraPassObj.RenderPass = GraphicInterface->CreateRenderPass(MotionFormat, UHTransitionInfo());
	MotionObjectPassObj.RenderPass = GraphicInterface->CreateRenderPass(MotionFormat
		, UHTransitionInfo(VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		, DepthFormat);
}

void UHDeferredShadingRenderer::ReleaseRenderPassObjects(bool bFrameBufferOnly)
{
	VkDevice LogicalDevice = GraphicInterface->GetLogicalDevice();

	if (!bFrameBufferOnly)
	{
		BasePassObj.Release(LogicalDevice);
		LightPassObj.Release(LogicalDevice);
		SkyboxPassObj.Release(LogicalDevice);
		MotionCameraPassObj.Release(LogicalDevice);
		MotionObjectPassObj.Release(LogicalDevice);

		for (UHRenderPassObject& P : PostProcessPassObj)
		{
			P.ReleaseFrameBuffer(LogicalDevice);
		}
	}
	else
	{
		BasePassObj.ReleaseFrameBuffer(LogicalDevice);
		LightPassObj.ReleaseFrameBuffer(LogicalDevice);
		SkyboxPassObj.ReleaseFrameBuffer(LogicalDevice);
		MotionCameraPassObj.ReleaseFrameBuffer(LogicalDevice);
		MotionObjectPassObj.ReleaseFrameBuffer(LogicalDevice);

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
		SystemConstantsGPU[Idx]->CreaetBuffer(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

		ObjectConstantsGPU[Idx] = GraphicInterface->RequestRenderBuffer<UHObjectConstants>();
		ObjectConstantsGPU[Idx]->CreaetBuffer(CurrentScene->GetAllRendererCount(), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

		MaterialConstantsGPU[Idx] = GraphicInterface->RequestRenderBuffer<UHMaterialConstants>();
		MaterialConstantsGPU[Idx]->CreaetBuffer(CurrentScene->GetMaterialCount(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

		DirectionalLightBuffer[Idx] = GraphicInterface->RequestRenderBuffer<UHDirectionalLightConstants>();
		DirectionalLightBuffer[Idx]->CreaetBuffer(CurrentScene->GetDirLightCount(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
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

		UH_SAFE_RELEASE(MaterialConstantsGPU[Idx]);
		MaterialConstantsGPU[Idx].reset();
	}
}

void UHDeferredShadingRenderer::ResizeRTBuffers()
{
	if (GEnableRayTracing)
	{
		GraphicInterface->WaitGPU();
		GraphicInterface->RequestReleaseRT(RTShadowResult);

		int32_t ShadowQuality = std::clamp(ConfigInterface->RenderingSetting().RTDirectionalShadowQuality, 0, 2);
		RTShadowExtent.width = RenderResolution.width >> ShadowQuality;
		RTShadowExtent.height = RenderResolution.height >> ShadowQuality;
		RTShadowResult = GraphicInterface->RequestRenderTexture("RTShadowResult", RTShadowExtent, RTShadowFormat, true, true);

		// need to rewrite descriptors after resize
		UpdateDescriptors();
	}
}