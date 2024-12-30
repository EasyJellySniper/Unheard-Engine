#include "DeferredShadingRenderer.h"
#include "DescriptorHelper.h"
#include <unordered_set>
#include "../Engine/Engine.h"

// all init/create/release implementations of DeferredShadingRenderer are put here
#if WITH_EDITOR
UHDeferredShadingRenderer* UHDeferredShadingRenderer::SceneRendererEditorOnly = nullptr;
#endif

UHDeferredShadingRenderer::UHDeferredShadingRenderer(UHEngine* InEngine)
	: GraphicInterface(InEngine->GetGfx())
	, AssetManagerInterface(InEngine->GetAssetManager())
	, ConfigInterface(InEngine->GetConfigManager())
	, TimerInterface(InEngine->GetGameTimer())
	, RenderResolution(VkExtent2D())
	, RTShadowExtent(VkExtent2D())
	, CurrentFrameGT(0)
	, CurrentFrameRT(0)
	, FrameNumberRT(0)
	, bIsResetNeededShared(false)
	, bEnableAsyncComputeRT(false)
	, CurrentScene(nullptr)
	, SystemConstantsCPU(UHSystemConstants())
	, CubeMesh(nullptr)
	, DefaultSamplerIndex(UHINDEXNONE)
	, LinearClampSamplerIndex(UHINDEXNONE)
	, SkyCubeSamplerIndex(UHINDEXNONE)
	, PointClampSamplerIndex(UHINDEXNONE)
	, OpaqueSceneTextureIndex(UHINDEXNONE)
	, PostProcessResultIdx(0)
	, bIsTemporalReset(true)
	, RTInstanceCount(0)
	, NumWorkerThreads(0)
	, RenderThread(nullptr)
	, bVsyncRT(false)
	, bIsSwapChainResetGT(false)
	, bIsSwapChainResetRT(false)
	, bIsRenderingEnabledRT(true)
	, bIsSkyLightEnabledRT(true)
	, bNeedGenerateSH9RT(true)
	, LightCullingTileSize(16)
	, MaxPointLightPerTile(32)
	, MaxSpotLightPerTile(32)
#if WITH_EDITOR
	, DebugViewIndex(0)
	, RenderThreadTime(0)
	, DrawCalls(0)
	, OccludedCalls(0)
	, EditorWidthDelta(0)
	, EditorHeightDelta(0)
	, bDrawDebugViewRT(true)
#endif
	, bHasRefractionMaterialGT(false)
	, bHasRefractionMaterialRT(false)
	, bHDREnabledRT(false)
	, RTCullingDistanceRT(0.0f)
	, RTReflectionQualityRT(0)
	, bIsRaytracingEnableRT(false)
	, bEnableHWOcclusionRT(false)
	, bEnableDepthPrepassRT(false)
	, OcclusionThresholdRT(0)
	, MeshInstanceCount(0)
{
	for (int32_t Idx = 0; Idx < NumOfPostProcessRT; Idx++)
	{
		PostProcessResults[Idx] = nullptr;
	}

	for (int32_t Idx = 0; Idx < UH_ENUM_VALUE(UHRenderPassTypes::UHRenderPassMax); Idx++)
	{
		GPUTimeQueries[Idx] = nullptr;
	}

	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		OcclusionQuery[Idx] = nullptr;
	}

#if WITH_EDITOR
	SceneRendererEditorOnly = this;
#endif
}

bool UHDeferredShadingRenderer::Initialize(UHScene* InScene)
{
	// scene setup
	CurrentScene = InScene;

	// config setup
	NumWorkerThreads = ConfigInterface->RenderingSetting().ParallelThreads;

	const bool bIsRendererSuccess = InitQueueSubmitters();
	if (bIsRendererSuccess)
	{
		// create render pass stuffs
		CreateRenderingBuffers();
		CreateRenderPasses();
		CreateRenderFrameBuffers();

		// create thread objects
		CreateThreadObjects();

		InitGaussianConstants();

		// reserve enough space for renderers, save the allocation time
		OpaquesToRender.reserve(CurrentScene->GetOpaqueRenderers().size());
		MotionOpaquesToRender.reserve(CurrentScene->GetOpaqueRenderers().size());
		TranslucentsToRender.reserve(CurrentScene->GetTranslucentRenderers().size());
		OcclusionRenderers.reserve(CurrentScene->GetAllRendererCount());

		for (int32_t Idx = 0; Idx < MaxCountingElement; Idx++)
		{
			CountingRenderers[Idx].reserve(1024);
		}
	}

	return bIsRendererSuccess;
}

void UHDeferredShadingRenderer::InitRenderingResources()
{
	PrepareMeshes();
	PrepareTextures();
	PrepareSamplers();
	GSkyLightCube = GetCurrentSkyCube();

	PrepareRenderingShaders();

	// create data buffers
	CreateDataBuffers();

	// update descriptor binding
	UpdateDescriptors();
}

void UHDeferredShadingRenderer::Release()
{
	VkDevice LogicalDevice = GraphicInterface->GetLogicalDevice();

	// wait device to finish before release
	GraphicInterface->WaitGPU();

	// end threads
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

	SceneRenderQueue.Release();
	TranslucentParallelSubmitter.Release();

	if (GIsEditor || (ConfigInterface->RenderingSetting().bEnableDepthPrePass && !GraphicInterface->IsMeshShaderSupported()))
	{
		DepthParallelSubmitter.Release();
	}

	if (!GraphicInterface->IsMeshShaderSupported())
	{
		BaseParallelSubmitter.Release();
		MotionOpaqueParallelSubmitter.Release();
		MotionTranslucentParallelSubmitter.Release();
	}

	if (GIsEditor || ConfigInterface->RenderingSetting().bEnableHardwareOcclusion)
	{
		ReleaseOcclusionQuery();
		OcclusionParallelSubmitter.Release();
	}

	if (ConfigInterface->RenderingSetting().bEnableAsyncCompute)
	{
		ReleaseAsyncComputeQueue();
	}

	UHOcclusionPassShader::ResetOcclusionState();
	UHShaderClass::ClearGlobalLayoutCache(GraphicInterface);
}

void UHDeferredShadingRenderer::PrepareMeshes()
{
	// create mesh buffer for all default lit renderers
	const std::vector<UHMeshRendererComponent*>& Renderers = CurrentScene->GetAllRenderers();

	// needs the cmd buffer
	VkCommandBuffer CreationCmd = GraphicInterface->BeginOneTimeCmd();
	CubeMesh = AssetManagerInterface->GetMesh("UHMesh_Cube");
	CubeMesh->CreateGPUBuffers(GraphicInterface);

	std::unordered_set<uint32_t> MeshTable;
	MeshInstanceCount = 0;
	MeshInUse.clear();

	for (const UHMeshRendererComponent* Renderer : Renderers)
	{
		UHMesh* Mesh = Renderer->GetMesh();
		UHMaterial* Mat = Renderer->GetMaterial();
		Mesh->CreateGPUBuffers(GraphicInterface);

		if (GraphicInterface->IsRayTracingEnabled())
		{
			Mesh->CreateBottomLevelAS(GraphicInterface, CreationCmd);
		}

		// assign buffer data index
		if (MeshTable.find(Mesh->GetId()) == MeshTable.end())
		{
			MeshTable.insert(Mesh->GetId());
			Mesh->SetBufferDataIndex(MeshInstanceCount++);
			MeshInUse.push_back(Mesh);
		}
	}
	GraphicInterface->EndOneTimeCmd(CreationCmd);

	// create top level AS after bottom level AS is done
	// can't be created in the same command line!! All bottom level AS must be created before creating top level AS
	if (GraphicInterface->IsRayTracingEnabled())
	{
		CreationCmd = GraphicInterface->BeginOneTimeCmd();
		for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
		{
			UH_SAFE_RELEASE(GTopLevelAS[Idx]);
			GTopLevelAS[Idx] = GraphicInterface->RequestAccelerationStructure();
			RTInstanceCount = GTopLevelAS[Idx]->CreateTopAS(Renderers, CreationCmd);
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

	// create mesh tables
	RecreateMeshTables();

	// release CPU copy of meshes for shipping
	if (GIsShipping)
	{
		for (UHMesh* Mesh : AssetManagerInterface->GetUHMeshes())
		{
			Mesh->ReleaseCPUMeshData();
		}
	}
}

void UHDeferredShadingRenderer::CheckTextureReference(std::vector<UHMaterial*> InMats)
{
	VkCommandBuffer CreationCmd = GraphicInterface->BeginOneTimeCmd();
	UHRenderBuilder RenderBuilder(GraphicInterface, CreationCmd);

	for (size_t Idx = 0; Idx < InMats.size(); Idx++)
	{
		for (const std::string RegisteredTexture : InMats[Idx]->GetRegisteredTextureNames())
		{
			UHTexture2D* Tex = AssetManagerInterface->GetTexture2D(RegisteredTexture);
			if (Tex)
			{
				Tex->UploadToGPU(GraphicInterface, RenderBuilder);
			}
		}

		AssetManagerInterface->MapTextureIndex(InMats[Idx]);
	}
	GraphicInterface->EndOneTimeCmd(CreationCmd);
}

void UHDeferredShadingRenderer::PrepareTextures()
{
	// instead of uploading to GPU right after import, I choose to upload textures if they're really in use
	// this makes workflow complicated but good for GPU memory


	// Step1: uploading all textures which are really used for rendering
	std::vector<UHMaterial*> Mats;
	std::unordered_set<uint32_t> MatTable;

	for (const UHMeshRendererComponent* Renderer : CurrentScene->GetAllRenderers())
	{
		// gather the materials
		UHMaterial* Mat = Renderer->GetMaterial();
		if (MatTable.find(Mat->GetId()) == MatTable.end())
		{
			MatTable.insert(Mat->GetId());
			Mats.push_back(Mat);
		}
	}

	CheckTextureReference(Mats);
	
	VkCommandBuffer CreationCmd = GraphicInterface->BeginOneTimeCmd();
	UHRenderBuilder RenderBuilder(GraphicInterface, CreationCmd);

	// Step2: Build all cubemaps in use
	if (UHTextureCube* Cube = GetCurrentSkyCube())
	{
		Cube->Build(GraphicInterface, RenderBuilder);
	}


	// Next, allocate system fallback textures
	const uint32_t FallbackWidth = 2;
	const uint32_t FallbackHeight = 2;
	const UHTextureFormat FallbackTexFormat = UHTextureFormat::UH_FORMAT_RGBA8_UNORM;

	// textures
	{
		std::vector<uint8_t> TexData(2 * 2 * GTextureFormatData[UH_ENUM_VALUE(FallbackTexFormat)].ByteSize, 255);
		GWhiteTexture = CreateTexture2D(GraphicInterface, FallbackWidth, FallbackHeight, FallbackTexFormat, "SystemWhiteTex");
		GWhiteTexture->SetTextureData(TexData);
		GWhiteTexture->UploadToGPU(GraphicInterface, RenderBuilder);

		memset(TexData.data(), 0, TexData.size());
		GBlackTexture = CreateTexture2D(GraphicInterface, FallbackWidth, FallbackHeight, FallbackTexFormat, "SystemBlackTex");
		GBlackTexture->SetTextureData(TexData);
		GBlackTexture->UploadToGPU(GraphicInterface, RenderBuilder);
	}

	// cubemaps
	{
		GBlackCube = CreateTextureCube(GraphicInterface, FallbackWidth, FallbackHeight, FallbackTexFormat, "SystemBlackCube");
		GBlackCube->Build(GraphicInterface, RenderBuilder);
	}

	GraphicInterface->EndOneTimeCmd(CreationCmd);

	// release CPU texture data for shipping
	if (GIsShipping)
	{
		for (UHTexture2D* Tex : AssetManagerInterface->GetTexture2Ds())
		{
			Tex->ReleaseCPUTextureData();
		}

		for (UHTextureCube* Cube : AssetManagerInterface->GetCubemaps())
		{
			Cube->ReleaseCPUData();
		}
	}
}

void UHDeferredShadingRenderer::PrepareSamplers()
{
	// shared sampler requests, cache their sampler index when necessary

	UHSamplerInfo PointClampedInfo(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	PointClampedInfo.MaxAnisotropy = 1;
	GPointClampedSampler = GraphicInterface->RequestTextureSampler(PointClampedInfo);
	PointClampSamplerIndex = UHUtilities::FindIndex(GraphicInterface->GetSamplers(), GPointClampedSampler);

#if WITH_EDITOR
	// request a sampler for preview scene
	PointClampedInfo.MipBias = 0;
	GraphicInterface->RequestTextureSampler(PointClampedInfo);
#endif

	UHSamplerInfo LinearClampedInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	LinearClampedInfo.MaxAnisotropy = 1;
	GLinearClampedSampler = GraphicInterface->RequestTextureSampler(LinearClampedInfo);
	LinearClampSamplerIndex = UHUtilities::FindIndex(GraphicInterface->GetSamplers(), GLinearClampedSampler);

	LinearClampedInfo.MaxAnisotropy = 16;
	UHSampler* AnisoClampedSampler = GraphicInterface->RequestTextureSampler(LinearClampedInfo);
	DefaultSamplerIndex = UHUtilities::FindIndex(GraphicInterface->GetSamplers(), AnisoClampedSampler);

	GSkyCubeSampler = GraphicInterface->RequestTextureSampler(UHSamplerInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT
		, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT));
	SkyCubeSamplerIndex = UHUtilities::FindIndex(GraphicInterface->GetSamplers(), GSkyCubeSampler);
}

void UHDeferredShadingRenderer::PrepareRenderingShaders()
{
	// bindless table
	TextureTable = MakeUnique<UHTextureTable>(GraphicInterface, "TextureTable");
	SamplerTable = MakeUnique<UHSamplerTable>(GraphicInterface, "SamplerTable");
	const std::vector<VkDescriptorSetLayout> BindlessLayouts = { TextureTable->GetDescriptorSetLayout(), SamplerTable->GetDescriptorSetLayout()};
	const std::vector<UHMeshRendererComponent*> AllRenderers = CurrentScene->GetAllRenderers();

	// create all material shaders
	if (!GraphicInterface->IsMeshShaderSupported())
	{
		if (GIsEditor || ConfigInterface->RenderingSetting().bEnableDepthPrePass)
		{
			DepthPassShaders.reserve(std::numeric_limits<int16_t>::max());
		}
		BasePassShaders.reserve(std::numeric_limits<int16_t>::max());
		MotionOpaqueShaders.reserve(std::numeric_limits<int16_t>::max());
		MotionTranslucentShaders.reserve(std::numeric_limits<int16_t>::max());
	}
	TranslucentPassShaders.reserve(std::numeric_limits<int16_t>::max());

	for (UHMeshRendererComponent* Renderer : AllRenderers)
	{
		UHMaterial* Mat = Renderer->GetMaterial();
		if (Mat)
		{
			RecreateMaterialShaders(Renderer, Mat);
		}
	}

	// create occlusion shaders if enabled
	if (GIsEditor || ConfigInterface->RenderingSetting().bEnableHardwareOcclusion)
	{
		OcclusionPassShaders.resize(CurrentScene->GetAllRendererCount());
		for (size_t Idx = 0; Idx < AllRenderers.size(); Idx++)
		{
			UHMaterial* Mat = AllRenderers[Idx]->GetMaterial();
			if (Mat)
			{
				OcclusionPassShaders[Idx] = MakeUnique<UHOcclusionPassShader>(GraphicInterface, "OcclusionPassShader", OcclusionPassObj.RenderPass);
			}
		}
	}

	// create mesh shaders if supported
	if (GraphicInterface->IsMeshShaderSupported())
	{
		MeshShaderInstancesCounter.resize(CurrentScene->GetMaterialCount());
		SortedMeshShaderGroupIndex.resize(CurrentScene->GetMaterialCount());
		VisibleMeshShaderData.resize(CurrentScene->GetMaterialCount());
		MotionOpaqueMeshShaderData.resize(CurrentScene->GetMaterialCount());
		MotionTranslucentMeshShaderData.resize(CurrentScene->GetMaterialCount());

		for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
		{
			GMeshShaderData[Idx].resize(CurrentScene->GetMaterialCount());
			GMotionOpaqueShaderData[Idx].resize(CurrentScene->GetMaterialCount());
			GMotionTranslucentShaderData[Idx].resize(CurrentScene->GetMaterialCount());
		}

		DepthMeshShaders.resize(CurrentScene->GetMaterialCount());
		BaseMeshShaders.resize(CurrentScene->GetMaterialCount());
		MotionMeshShaders.resize(CurrentScene->GetMaterialCount());

		for (UHMaterial* Mat : CurrentScene->GetMaterials())
		{
			RecreateMeshShaders(Mat);
		}
	}

	// light culling and pass shaders
	LightCullingShader = MakeUnique<UHLightCullingShader>(GraphicInterface, "LightCullingShader");
	LightPassShader = MakeUnique<UHLightPassShader>(GraphicInterface, "LightPassShader");
	ReflectionPassShader = MakeUnique<UHReflectionPassShader>(GraphicInterface, "ReflectionPassShader");
	RTReflectionMipmapShader = MakeUnique<UHRTReflectionMipmap>(GraphicInterface, "RTReflectionMipmapShader");

	// sky pass shader
	SkyPassShader = MakeUnique<UHSkyPassShader>(GraphicInterface, "SkyPassShader", SkyboxPassObj.RenderPass);
	SH9Shader = MakeUnique<UHSphericalHarmonicShader>(GraphicInterface, "SH9Shader");

	// motion pass shader
	MotionCameraShader = MakeUnique<UHMotionCameraPassShader>(GraphicInterface, "MotionCameraPassShader", MotionCameraPassObj.RenderPass);

	// post processing shaders
	TemporalAAShader = MakeUnique<UHTemporalAAShader>(GraphicInterface, "TemporalAAShader");
	ToneMapShader = MakeUnique<UHToneMappingShader>(GraphicInterface, "ToneMapShader", PostProcessPassObj[0].RenderPass);

	GaussianFilterHShader = MakeUnique<UHGaussianFilterShader>(GraphicInterface, "FilterHShader", UHGaussianFilterType::FilterHorizontal);
	GaussianFilterVShader = MakeUnique<UHGaussianFilterShader>(GraphicInterface, "FilterVShader", UHGaussianFilterType::FilterVertical);

	// RT shaders
	if (GraphicInterface->IsRayTracingEnabled() && RTInstanceCount > 0)
	{
		RecreateRTShaders(std::vector<UHMaterial*>(), true);
		SoftRTShadowShader = MakeUnique<UHSoftRTShadowShader>(GraphicInterface, "SoftRTShadowShader");
		CollectPointLightShader = MakeUnique<UHCollectLightShader>(GraphicInterface, "CollectPointLightShader", true);
		CollectSpotLightShader = MakeUnique<UHCollectLightShader>(GraphicInterface, "CollectSpotLightShader", false);
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
	if (ConfigInterface->RenderingSetting().bEnableAsyncCompute)
	{
		bAsyncInitSucceed &= CreateAsyncComputeQueue();
	}

	return bAsyncInitSucceed && SceneRenderQueue.Initialize(GraphicInterface, QueueFamily.GraphicsFamily.value(), 0
		, "GraphicQueue");
}

void UHDeferredShadingRenderer::UpdateDescriptors()
{
	VkDevice LogicalDevice = GraphicInterface->GetLogicalDevice();
	GSkyLightCube = GetCurrentSkyCube();
	const bool bEnableRayTracing = ConfigInterface->RenderingSetting().bEnableRayTracing && GraphicInterface->IsRayTracingEnabled();

	// ------------------------------------------------ Bindless table update
	UpdateTextureDescriptors();

	// ------------------------------------------------ Mesh shader descriptor update
	if (GraphicInterface->IsMeshShaderSupported())
	{
		for (size_t Idx = 0; Idx < CurrentScene->GetMaterialCount(); Idx++)
		{
			if (DepthMeshShaders[Idx] != nullptr)
			{
				DepthMeshShaders[Idx]->BindParameters();
			}

			if (BaseMeshShaders[Idx] != nullptr)
			{
				BaseMeshShaders[Idx]->BindParameters();
			}

			if (MotionMeshShaders[Idx] != nullptr)
			{
				MotionMeshShaders[Idx]->BindParameters();
			}
		}
	}
	else
	{
		// ------------------------------------------------ Depth pass descriptor update
		// always create depth pass stuff for toggling in editor
		if (GIsEditor || GraphicInterface->IsDepthPrePassEnabled())
		{
			for (const UHMeshRendererComponent* Renderer : CurrentScene->GetOpaqueRenderers())
			{
				assert(DepthPassShaders.find(Renderer->GetBufferDataIndex()) != DepthPassShaders.end());
				DepthPassShaders[Renderer->GetBufferDataIndex()]->BindParameters(Renderer);
			}
		}

		// ------------------------------------------------ Base pass descriptor update
		for (const UHMeshRendererComponent* Renderer : CurrentScene->GetOpaqueRenderers())
		{
			assert(BasePassShaders.find(Renderer->GetBufferDataIndex()) != BasePassShaders.end());
			BasePassShaders[Renderer->GetBufferDataIndex()]->BindParameters(Renderer);
		}
	}

	// ------------------------------------------------ Occlusion shader descriptor update
	if (GIsEditor || ConfigInterface->RenderingSetting().bEnableHardwareOcclusion)
	{
		for (const UHMeshRendererComponent* Renderer : CurrentScene->GetAllRenderers())
		{
			if (OcclusionPassShaders[Renderer->GetBufferDataIndex()] != nullptr)
			{
				OcclusionPassShaders[Renderer->GetBufferDataIndex()]->BindParameters(Renderer);
			}
		}
	}

	// ------------------------------------------------ Lighting culling descriptor update
	LightCullingShader->BindParameters();

	// ------------------------------------------------ Lighting pass descriptor update
	LightPassShader->BindParameters(bEnableRayTracing);
	ReflectionPassShader->BindParameters(bEnableRayTracing);

	// ------------------------------------------------ sky pass descriptor update
	SH9Shader->BindParameters();
	SkyPassShader->BindParameters();

	// ------------------------------------------------ motion pass descriptor update
	MotionCameraShader->BindParameters();

	for (const UHMeshRendererComponent* Renderer : CurrentScene->GetAllRenderers())
	{
		UHMaterial* Mat = Renderer->GetMaterial();
		if (Mat == nullptr)
		{
			continue;
		}

		const int32_t RendererIdx = Renderer->GetBufferDataIndex();
		if (!GraphicInterface->IsMeshShaderSupported())
		{
			if (Mat->IsOpaque())
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
	}

	// ------------------------------------------------ translucent pass descriptor update
	for (const UHMeshRendererComponent* Renderer : CurrentScene->GetTranslucentRenderers())
	{
		assert(TranslucentPassShaders.find(Renderer->GetBufferDataIndex()) != TranslucentPassShaders.end());
		TranslucentPassShaders[Renderer->GetBufferDataIndex()]->BindParameters(Renderer, bEnableRayTracing);
	}

	// ------------------------------------------------ post process pass descriptor update
	// when binding post processing input, be sure to bind it alternately, the two RT will keep bliting to each other
	// the post process RT binding will be in PostProcessRendering.cpp
	ToneMapShader->BindParameters();
	TemporalAAShader->BindParameters();

	// ------------------------------------------------ ray tracing pass descriptor update
	if (GraphicInterface->IsRayTracingEnabled() && RTInstanceCount > 0)
	{
		RTMeshInstanceTable->BindStorage(GRendererInstanceBuffer.get(), 0, 0, true);

		// bind RT material data
		for (int32_t FrameIdx = 0; FrameIdx < GMaxFrameInFlight; FrameIdx++)
		{
			std::vector<UHRenderBuffer<UHRTMaterialData>*> RTMaterialData;
			const std::vector<UHMaterial*>& AllMaterials = CurrentScene->GetMaterials();
			for (size_t Idx = 0; Idx < AllMaterials.size(); Idx++)
			{
				RTMaterialData.push_back(AllMaterials[Idx]->GetRTMaterialDataGPU(FrameIdx));
			}
			RTMaterialDataTable->BindStorage(RTMaterialData, 0, FrameIdx);
		}

		// bind textures used by RT
		std::vector<UHTexture*> Textures;
		Textures.push_back(GSceneDiffuse);
		Textures.push_back(GSceneNormal);
		Textures.push_back(GSceneMaterial);
		Textures.push_back(GSceneDepth);
		Textures.push_back(GSceneVertexNormal);
		RTTextureTable->BindImage(Textures, 0);

		SoftRTShadowShader->BindParameters();
		RTShadowShader->BindParameters();
		RTReflectionShader->BindParameters();
		CollectPointLightShader->BindParameters();
		CollectSpotLightShader->BindParameters();
	}

	// ------------------------------------------------ mesh table descriptor update
	if ((GraphicInterface->IsRayTracingEnabled() && RTInstanceCount > 0)
		|| (GraphicInterface->IsMeshShaderSupported() && MeshInstanceCount > 0))
	{
		// bind VB/IB table for RT or mesh shader use
		std::vector<UHRenderBuffer<XMFLOAT3>*> Positions;
		std::vector<UHRenderBuffer<XMFLOAT2>*> UVs;
		std::vector<UHRenderBuffer<XMFLOAT3>*> Normals;
		std::vector<UHRenderBuffer<XMFLOAT4>*> Tangents;
		std::vector<VkDescriptorBufferInfo> IndicesInfo;
		std::vector<UHRenderBuffer<UHMeshlet>*> Meshlets;

		// setup mesh data array to bind
		for (uint32_t Idx = 0; Idx < MeshInstanceCount; Idx++)
		{
			const UHMesh* Mesh = MeshInUse[Idx];
			Positions.push_back(Mesh->GetPositionBuffer());
			UVs.push_back(Mesh->GetUV0Buffer());
			Normals.push_back(Mesh->GetNormalBuffer());
			Tangents.push_back(Mesh->GetTangentBuffer());
			Meshlets.push_back(Mesh->GetMeshletBuffer());

			// collect index buffer info based on index type
			VkDescriptorBufferInfo NewInfo{};
			if (Mesh->IsIndexBufer32Bit())
			{
				NewInfo.buffer = Mesh->GetIndexBuffer()->GetBuffer();
				NewInfo.range = Mesh->GetIndexBuffer()->GetBufferSize();
			}
			else
			{
				NewInfo.buffer = Mesh->GetIndexBuffer16()->GetBuffer();
				NewInfo.range = Mesh->GetIndexBuffer16()->GetBufferSize();
			}
			IndicesInfo.push_back(NewInfo);
		}

		PositionTable->BindStorage(Positions, 0);
		UV0Table->BindStorage(UVs, 0);
		NormalTable->BindStorage(Normals, 0);
		TangentTable->BindStorage(Tangents, 0);
		IndicesTable->BindStorage(IndicesInfo, 0);
		MeshletTable->BindStorage(Meshlets, 0);
	}

	// ------------------------------------------------ debug passes descriptor update
#if WITH_EDITOR
	DebugViewShader->BindParameters();
	DebugBoundShader->BindParameters();

	// refresh the debug view too
	SetDebugViewIndex(DebugViewIndex);
#endif
}

void UHDeferredShadingRenderer::ReleaseShaders()
{
	if (GIsEditor || GraphicInterface->IsDepthPrePassEnabled())
	{
		ClearContainer(DepthPassShaders);
		ClearContainer(DepthMeshShaders);
	}

	ClearContainer(BasePassShaders);
	ClearContainer(BaseMeshShaders);
	ClearContainer(MotionOpaqueShaders);
	ClearContainer(MotionTranslucentShaders);
	ClearContainer(MotionMeshShaders);
	ClearContainer(TranslucentPassShaders);
	ClearContainer(OcclusionPassShaders);

	UH_SAFE_RELEASE(LightCullingShader);
	UH_SAFE_RELEASE(LightPassShader);
	UH_SAFE_RELEASE(ReflectionPassShader);
	UH_SAFE_RELEASE(RTReflectionMipmapShader);
	UH_SAFE_RELEASE(SkyPassShader);
	UH_SAFE_RELEASE(SH9Shader);
	UH_SAFE_RELEASE(MotionCameraShader);
	UH_SAFE_RELEASE(TemporalAAShader);
	UH_SAFE_RELEASE(ToneMapShader);
	UH_SAFE_RELEASE(GaussianFilterHShader);
	UH_SAFE_RELEASE(GaussianFilterVShader);

	if (GraphicInterface->IsMeshShaderSupported() || GraphicInterface->IsRayTracingEnabled())
	{
		UH_SAFE_RELEASE(PositionTable);
		UH_SAFE_RELEASE(UV0Table);
		UH_SAFE_RELEASE(NormalTable);
		UH_SAFE_RELEASE(TangentTable);
		UH_SAFE_RELEASE(IndicesTable);
		UH_SAFE_RELEASE(MeshletTable);
	}

	if (GraphicInterface->IsRayTracingEnabled())
	{
		UH_SAFE_RELEASE(RTDefaultHitGroupShader);
		UH_SAFE_RELEASE(RTShadowShader);
		UH_SAFE_RELEASE(RTReflectionShader);
		UH_SAFE_RELEASE(RTMeshInstanceTable);
		UH_SAFE_RELEASE(RTMaterialDataTable);
		UH_SAFE_RELEASE(RTTextureTable);
		UH_SAFE_RELEASE(SoftRTShadowShader);
		UH_SAFE_RELEASE(CollectPointLightShader);
		UH_SAFE_RELEASE(CollectSpotLightShader);
	}

	UH_SAFE_RELEASE(TextureTable);
	UH_SAFE_RELEASE(SamplerTable);

#if WITH_EDITOR
	UH_SAFE_RELEASE(DebugViewShader);
	UH_SAFE_RELEASE(DebugBoundShader);
#endif
}

void UHDeferredShadingRenderer::CreateRenderingBuffers()
{
	// init formats
	const UHTextureFormat DiffuseFormat = UHTextureFormat::UH_FORMAT_RGBA8_SRGB;
	const UHTextureFormat NormalFormat = UHTextureFormat::UH_FORMAT_A2R10G10B10;
	const UHTextureFormat SpecularFormat = UHTextureFormat::UH_FORMAT_RGBA8_UNORM;
	const UHTextureFormat SceneResultFormat = UHTextureFormat::UH_FORMAT_RGBA16F;
	const UHTextureFormat HistoryResultFormat = UHTextureFormat::UH_FORMAT_R11G11B10;
	const UHTextureFormat SceneMipFormat = UHTextureFormat::UH_FORMAT_R16F;
	const UHTextureFormat DepthFormat = (GraphicInterface->Is24BitDepthSupported()) ? UHTextureFormat::UH_FORMAT_X8_D24 : UHTextureFormat::UH_FORMAT_D32F;
	const UHTextureFormat MotionFormat = UHTextureFormat::UH_FORMAT_RG16F;
	const UHTextureFormat MaskFormat = UHTextureFormat::UH_FORMAT_R8_UNORM;

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

	// buffers for saving the "search ray" in RT shadows
	GSceneVertexNormal = GraphicInterface->RequestRenderTexture("SceneVertexNormal", RenderResolution, NormalFormat);
	GTranslucentBump = GraphicInterface->RequestRenderTexture("TranslucentBump", RenderResolution, NormalFormat);
	GTranslucentSmoothness = GraphicInterface->RequestRenderTexture("TranslucentSmoothness", RenderResolution, MaskFormat);

	// post process buffer, use the same format as scene result
	GPostProcessRT = GraphicInterface->RequestRenderTexture("PostProcessRT", RenderResolution, SceneResultFormat, true);
	GPreviousSceneResult = GraphicInterface->RequestRenderTexture("PreviousResultRT", RenderResolution, HistoryResultFormat);
	GOpaqueSceneResult = GraphicInterface->RequestRenderTexture("OpaqueSceneResult", RenderResolution, HistoryResultFormat);

	// motion vector buffer
	GMotionVectorRT = GraphicInterface->RequestRenderTexture("MotionVectorRT", RenderResolution, MotionFormat);

	// rt shadows buffer
	ResizeRayTracingBuffers(true);

	// create light culling tile buffer
	uint32_t TileCountX, TileCountY;
	GetLightCullingTileCount(TileCountX, TileCountY);

	// data size per tile: (MaxPointLightPerTile + 1), the extra one is for the number of lights in this tile
	// so it's TileCountX * TileCountY * (MaxPointLightPerTile + 1) in total
	GPointLightListBuffer = GraphicInterface->RequestRenderBuffer<uint32_t>(TileCountX * TileCountY * (MaxPointLightPerTile + 1), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		, "PointLightList");
	GPointLightListTransBuffer = GraphicInterface->RequestRenderBuffer<uint32_t>(TileCountX * TileCountY * (MaxPointLightPerTile + 1), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		, "PointLightListTrans");

	GSpotLightListBuffer = GraphicInterface->RequestRenderBuffer<uint32_t>(TileCountX * TileCountY * (MaxSpotLightPerTile + 1), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		, "SpotLightList");
	GSpotLightListTransBuffer = GraphicInterface->RequestRenderBuffer<uint32_t>(TileCountX * TileCountY * (MaxSpotLightPerTile + 1), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		, "SpotLightListTrans");
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
	GraphicInterface->RequestReleaseRT(GSceneVertexNormal);
	GraphicInterface->RequestReleaseRT(GPostProcessRT);
	GraphicInterface->RequestReleaseRT(GPreviousSceneResult);
	GraphicInterface->RequestReleaseRT(GOpaqueSceneResult);
	GraphicInterface->RequestReleaseRT(GMotionVectorRT);
	GraphicInterface->RequestReleaseRT(GTranslucentBump);
	GraphicInterface->RequestReleaseRT(GTranslucentSmoothness);

	ReleaseRayTracingBuffers();

	// point light list needs to be resized, so release it here instead in ReleaseDataBuffers()
	UH_SAFE_RELEASE(GPointLightListBuffer);
	UH_SAFE_RELEASE(GPointLightListTransBuffer);
	UH_SAFE_RELEASE(GSpotLightListBuffer);
	UH_SAFE_RELEASE(GSpotLightListTransBuffer);

	// cleanup gaussian constant
	RayTracingGaussianConsts.Release(GraphicInterface);
}

void UHDeferredShadingRenderer::CreateRenderPasses()
{
	// -------------------------------------------------------- Createing render pass after render pass is done -------------------------------------------------------- //
	std::vector<UHTexture*> GBufferTextures = { GSceneDiffuse ,GSceneNormal ,GSceneMaterial ,GSceneResult ,GSceneMip,GSceneVertexNormal };

	// depth prepass
	if (GIsEditor || GraphicInterface->IsDepthPrePassEnabled())
	{
		DepthPassObj = GraphicInterface->CreateRenderPass(UHTransitionInfo(), GSceneDepth);
	}

	if (GIsEditor || ConfigInterface->RenderingSetting().bEnableHardwareOcclusion)
	{
		// occlusion only needs depth load
		OcclusionPassObj = GraphicInterface->CreateRenderPass(UHTransitionInfo(VK_ATTACHMENT_LOAD_OP_LOAD), GSceneDepth);
	}

	// create render pass based on output RT, render pass can be shared sometimes
	BasePassObj = GraphicInterface->CreateRenderPass(GBufferTextures, UHTransitionInfo(GraphicInterface->IsDepthPrePassEnabled() ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR)
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

	std::vector<UHTexture*> TranslucentMotionTextures = { GMotionVectorRT , GSceneVertexNormal, GTranslucentBump, GTranslucentSmoothness, GSceneMip };
	MotionTranslucentPassObj = GraphicInterface->CreateRenderPass(TranslucentMotionTextures
		, UHTransitionInfo(VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		, GSceneDepth);
}

void UHDeferredShadingRenderer::CreateRenderFrameBuffers()
{
	// collect image views for creaing one frame buffer
	std::vector<UHRenderTexture*> GBuffers;
	GBuffers.push_back(GSceneDiffuse);
	GBuffers.push_back(GSceneNormal);
	GBuffers.push_back(GSceneMaterial);
	GBuffers.push_back(GSceneResult);
	GBuffers.push_back(GSceneMip);
	GBuffers.push_back(GSceneVertexNormal);
	GBuffers.push_back(GSceneDepth);

	// depth frame buffer
	if (GIsEditor || GraphicInterface->IsDepthPrePassEnabled())
	{
		DepthPassObj.FrameBuffer = GraphicInterface->CreateFrameBuffer(GSceneDepth, DepthPassObj.RenderPass, RenderResolution);
	}

	if (GIsEditor || ConfigInterface->RenderingSetting().bEnableHardwareOcclusion)
	{
		OcclusionPassObj.FrameBuffer = GraphicInterface->CreateFrameBuffer(GSceneDepth, OcclusionPassObj.RenderPass, RenderResolution);
	}

	// create frame buffer, some of them can be shared, especially when the output target is the same
	BasePassObj.FrameBuffer = GraphicInterface->CreateFrameBuffer(GBuffers, BasePassObj.RenderPass, RenderResolution);

	// sky pass need depth
	GBuffers = { GSceneResult , GSceneDepth };
	SkyboxPassObj.FrameBuffer = GraphicInterface->CreateFrameBuffer(GBuffers, SkyboxPassObj.RenderPass, RenderResolution);

	// translucent pass can share the same frame buffer as skybox pass
	TranslucentPassObj.FrameBuffer = SkyboxPassObj.FrameBuffer;

	// post process pass, use two buffer and blit each other
	PostProcessPassObj[0].FrameBuffer = GraphicInterface->CreateFrameBuffer(GPostProcessRT, PostProcessPassObj[0].RenderPass, RenderResolution);
	PostProcessPassObj[1].FrameBuffer = GraphicInterface->CreateFrameBuffer(GSceneResult, PostProcessPassObj[1].RenderPass, RenderResolution);

	// motion pass framebuffer
	MotionCameraPassObj.FrameBuffer = GraphicInterface->CreateFrameBuffer(GMotionVectorRT, MotionCameraPassObj.RenderPass, RenderResolution);

	// the opaque depth will be copied to translucent depth before motion opaque pass kicks off
	GBuffers = { GMotionVectorRT, GSceneTranslucentDepth };
	MotionOpaquePassObj.FrameBuffer = GraphicInterface->CreateFrameBuffer(GBuffers, MotionOpaquePassObj.RenderPass, RenderResolution);

	GBuffers = { GMotionVectorRT, GSceneVertexNormal, GTranslucentBump, GTranslucentSmoothness
		, GSceneMip, GSceneTranslucentDepth };
	MotionTranslucentPassObj.FrameBuffer = GraphicInterface->CreateFrameBuffer(GBuffers, MotionTranslucentPassObj.RenderPass, RenderResolution);
}

void UHDeferredShadingRenderer::ReleaseRenderPassObjects()
{
	VkDevice LogicalDevice = GraphicInterface->GetLogicalDevice();

	if (GIsEditor || GraphicInterface->IsDepthPrePassEnabled())
	{
		DepthPassObj.Release(LogicalDevice);
	}

	if (GIsEditor || ConfigInterface->RenderingSetting().bEnableHardwareOcclusion)
	{
		OcclusionPassObj.Release(LogicalDevice);
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

	const size_t RendererCount = CurrentScene->GetAllRendererCount();

	// create constants and buffers
	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		GSystemConstantBuffer[Idx] = GraphicInterface->RequestRenderBuffer<UHSystemConstants>(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
			, "SystemConstant");
		GObjectConstantBuffer[Idx] = GraphicInterface->RequestRenderBuffer<UHObjectConstants>(RendererCount, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
			, "ObjectConstant");
		GDirectionalLightBuffer[Idx] = GraphicInterface->RequestRenderBuffer<UHDirectionalLightConstants>(CurrentScene->GetDirLightCount(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
			, "DirectionalLight");
		GPointLightBuffer[Idx] = GraphicInterface->RequestRenderBuffer<UHPointLightConstants>(CurrentScene->GetPointLightCount(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
			, "PointLight");
		GSpotLightBuffer[Idx] = GraphicInterface->RequestRenderBuffer<UHSpotLightConstants>(CurrentScene->GetSpotLightCount(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
			, "SpotLight");
	}

	ObjectConstantsCPU.resize(RendererCount);
	DirLightConstantsCPU.resize(CurrentScene->GetDirLightCount());
	PointLightConstantsCPU.resize(CurrentScene->GetPointLightCount());
	SpotLightConstantsCPU.resize(CurrentScene->GetSpotLightCount());

	GSH9Data = GraphicInterface->RequestRenderBuffer<UHSphericalHarmonicData>(1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "SH9Data");

	if (GraphicInterface->IsMeshShaderSupported() || ConfigInterface->RenderingSetting().bEnableRayTracing)
	{
		const size_t TotalRenderers = CurrentScene->GetOpaqueRenderers().size() + CurrentScene->GetTranslucentRenderers().size();
		if (TotalRenderers == 0)
		{
			return;
		}

		// create renderer instance buffer
		UH_SAFE_RELEASE(GRendererInstanceBuffer);
		GRendererInstanceBuffer = GraphicInterface->RequestRenderBuffer<UHRendererInstance>(TotalRenderers, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
			, "RendererInstance");

		// collect & upload mesh instance data
		const std::vector<UHMeshRendererComponent*>& Renderers = CurrentScene->GetAllRenderers();
		std::vector<UHRendererInstance> RendererInstances(Renderers.size());

		for (int32_t Idx = 0; Idx < static_cast<int32_t>(Renderers.size()); Idx++)
		{
			const UHMaterial* Mat = Renderers[Idx]->GetMaterial();
			const UHMesh* Mesh = Renderers[Idx]->GetMesh();
			if (!Mat || !Mesh)
			{
				continue;
			}

			UHRendererInstance RendererInstance;
			RendererInstance.MeshIndex = Mesh->GetBufferDataIndex();
			RendererInstance.IndiceType = Mesh->IsIndexBufer32Bit() ? 1 : 0;
			RendererInstances[Renderers[Idx]->GetBufferDataIndex()] = RendererInstance;
		}

		GRendererInstanceBuffer->UploadAllData(RendererInstances.data());

		// create instance lights buffer
		if (GIsEditor || ConfigInterface->RenderingSetting().bEnableRayTracing)
		{
			GInstanceLightsBuffer = GraphicInterface->RequestRenderBuffer<UHInstanceLights>(Renderers.size()
				, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "InstanceLights");
		}
	}

	// create occlusion query anyway in editor build
	if (GIsEditor || ConfigInterface->RenderingSetting().bEnableHardwareOcclusion)
	{
		CreateOcclusionQuery();
	}
}

void UHDeferredShadingRenderer::CreateOcclusionQuery()
{
	const uint32_t Count = static_cast<uint32_t>(CurrentScene->GetAllRendererCount());
	if (Count > 0)
	{
		for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
		{
			// will do predication rendering on GPU instead of traditional readback
			OcclusionQuery[Idx] = GraphicInterface->RequestGPUQuery(Count, VK_QUERY_TYPE_OCCLUSION);
			GOcclusionResult[Idx] = GraphicInterface->RequestRenderBuffer<uint32_t>(Count
				, VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
				, "OcclusionResult");
		}

		for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
		{
			GOcclusionConstantBuffer[Idx] = GraphicInterface->RequestRenderBuffer<UHObjectConstants>(Count, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
				, "OcclusionConstant");
		}
		OcclusionConstantsCPU.resize(Count);

		std::vector<uint32_t> InitialVisibility(Count, 0);
		for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
		{
			GOcclusionResult[Idx]->UploadAllData(InitialVisibility.data());
		}
	}
}

void UHDeferredShadingRenderer::ReleaseOcclusionQuery()
{
	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		if (OcclusionQuery[Idx] != nullptr)
		{
			GraphicInterface->RequestReleaseGPUQuery(OcclusionQuery[Idx]);
			OcclusionQuery[Idx] = nullptr;
		}

		UH_SAFE_RELEASE(GOcclusionConstantBuffer[Idx]);
		UH_SAFE_RELEASE(GOcclusionResult[Idx]);
	}

	OcclusionConstantsCPU.clear();
}

bool UHDeferredShadingRenderer::CreateAsyncComputeQueue()
{
	VkDevice LogicalDevice = GraphicInterface->GetLogicalDevice();
	const UHQueueFamily& QueueFamily = GraphicInterface->GetQueueFamily();

	return AsyncComputeQueue.Initialize(GraphicInterface, QueueFamily.ComputesFamily.value(), 0, "AsyncComputeQueue");
}

void UHDeferredShadingRenderer::ReleaseAsyncComputeQueue()
{
	AsyncComputeQueue.Release();
}

void UHDeferredShadingRenderer::CreateThreadObjects()
{
#if WITH_EDITOR
	for (int32_t Idx = 0; Idx < UH_ENUM_VALUE(UHRenderPassTypes::UHRenderPassMax); Idx++)
	{
		GPUTimeQueries[Idx] = GraphicInterface->RequestGPUQuery(2, VK_QUERY_TYPE_TIMESTAMP);
	}
	ThreadDrawCalls.resize(NumWorkerThreads);
	ThreadOccludedCalls.resize(NumWorkerThreads);
#endif

	// create parallel submitter
	if (GIsEditor || (ConfigInterface->RenderingSetting().bEnableDepthPrePass && !GraphicInterface->IsMeshShaderSupported()))
	{
		DepthParallelSubmitter.Initialize(GraphicInterface, GraphicInterface->GetQueueFamily(), NumWorkerThreads
			, "DepthPass");
	}

	if (!GraphicInterface->IsMeshShaderSupported())
	{
		BaseParallelSubmitter.Initialize(GraphicInterface, GraphicInterface->GetQueueFamily(), NumWorkerThreads
			, "BasePass");
		MotionOpaqueParallelSubmitter.Initialize(GraphicInterface, GraphicInterface->GetQueueFamily(), NumWorkerThreads
			, "MotionOpaquePass");
		MotionTranslucentParallelSubmitter.Initialize(GraphicInterface, GraphicInterface->GetQueueFamily(), NumWorkerThreads
			, "MotionTranslucentPass");
	}

	TranslucentParallelSubmitter.Initialize(GraphicInterface, GraphicInterface->GetQueueFamily(), NumWorkerThreads
		, "TranslucentPass");

	if (GIsEditor || ConfigInterface->RenderingSetting().bEnableHardwareOcclusion)
	{
		OcclusionParallelSubmitter.Initialize(GraphicInterface, GraphicInterface->GetQueueFamily(), NumWorkerThreads
			, "OcclusionPass");
	}

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
		UH_SAFE_RELEASE(GTopLevelAS[Idx]);
	}

	UH_SAFE_RELEASE(GRendererInstanceBuffer);
	UH_SAFE_RELEASE(GSH9Data);
	UH_SAFE_RELEASE(GInstanceLightsBuffer);

	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		ClearContainer(GMeshShaderData[Idx]);
		ClearContainer(GMotionOpaqueShaderData[Idx]);
		ClearContainer(GMotionTranslucentShaderData[Idx]);
	}

	MeshShaderInstancesCounter.clear();
	SortedMeshShaderGroupIndex.clear();
	VisibleMeshShaderData.clear();
	MotionOpaqueMeshShaderData.clear();
	MotionTranslucentMeshShaderData.clear();
}

void UHDeferredShadingRenderer::UpdateTextureDescriptors()
{
	// ------------------------------------------------ Bindless table update
	std::vector<UHTexture*> Texes;

	// bind necessary textures from system, be sure to match the number of GSystemPreservedTextureSlots definition
	Texes.push_back(GOpaqueSceneResult);
	OpaqueSceneTextureIndex = 0;

	assert(GSystemPreservedTextureSlots == static_cast<int32_t>(Texes.size()));

	// bind textures from assets
	const std::vector<UHTexture2D*> AssetTextures = AssetManagerInterface->GetReferencedTexture2Ds();
	for (size_t Idx = 0; Idx < AssetTextures.size(); Idx++)
	{
		Texes.push_back(AssetTextures[Idx]);
	}

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

void UHDeferredShadingRenderer::InitGaussianConstants()
{
	UHTextureFormat TempRTFormat = bHDREnabledRT ? UHTextureFormat::UH_FORMAT_RGBA16F : UHTextureFormat::UH_FORMAT_RGBA8_UNORM;

	// initialize gaussian consts
	if (GraphicInterface->IsRayTracingEnabled())
	{
		RayTracingGaussianConsts.GBlurRadius = 2;
		RayTracingGaussianConsts.IterationCount = 2;
		CalculateBlurWeights(RayTracingGaussianConsts.GBlurRadius, RayTracingGaussianConsts.Weights);

		RayTracingGaussianConsts.GaussianFilterTempRT0.resize(GRTReflectionResult->GetMipMapCount());
		RayTracingGaussianConsts.GaussianFilterTempRT1.resize(GRTReflectionResult->GetMipMapCount());

		VkExtent2D FilterResolution;

		// for RT use
		for (uint32_t Idx = 2; Idx < GRTReflectionResult->GetMipMapCount(); Idx++)
		{
			FilterResolution.width = RenderResolution.width >> Idx;
			FilterResolution.height = RenderResolution.height >> Idx;

			RayTracingGaussianConsts.GaussianFilterTempRT0[Idx] =
				GraphicInterface->RequestRenderTexture("GaussianFilterTempRT0", FilterResolution, TempRTFormat, true);

			RayTracingGaussianConsts.GaussianFilterTempRT1[Idx] =
				GraphicInterface->RequestRenderTexture("GaussianFilterTempRT1", FilterResolution, TempRTFormat, true);
		}
	}
}

template <typename T1, typename T2>
void SafeReleaseShaderPtr(std::unordered_map<T1, T2>& InMap, const T1 InIndex, bool bDescriptorOnly)
{
	if (InMap.find(InIndex) != InMap.end())
	{
		bDescriptorOnly ? InMap[InIndex]->ReleaseDescriptor() : InMap[InIndex]->Release();
	}
}

#if WITH_EDITOR
UHDeferredShadingRenderer* UHDeferredShadingRenderer::GetRendererEditorOnly()
{
	return SceneRendererEditorOnly;
}

void UHDeferredShadingRenderer::RefreshMaterialShaders(UHMaterial* Mat, bool bNeedReassignRendererGroup, bool bDelayRTShaderCreation)
{
	// refresh material shader if necessary
	UHMaterialCompileFlag CompileFlag = Mat->GetCompileFlag();
	if (CompileFlag == UHMaterialCompileFlag::UpToDate)
	{
		return;
	}

	GraphicInterface->WaitGPU();
	CheckTextureReference(std::vector<UHMaterial*>{ Mat });

	const bool bIsOpaque = Mat->IsOpaque();
	const int32_t MatIndex = Mat->GetBufferDataIndex();

	for (UHObject* RendererObj : Mat->GetReferenceObjects())
	{
		if (UHMeshRendererComponent* Renderer = CastObject<UHMeshRendererComponent>(RendererObj))
		{
			ResetMaterialShaders(Renderer, CompileFlag, bIsOpaque, bNeedReassignRendererGroup);
			Renderer->SetRenderDirties(true);
		}
	}

	if (GraphicInterface->IsMeshShaderSupported())
	{
		if (CompileFlag == UHMaterialCompileFlag::StateChangedOnly)
		{
			if (DepthMeshShaders[MatIndex])
			{
				DepthMeshShaders[MatIndex]->RecreateMaterialState();
			}

			if (BaseMeshShaders[MatIndex])
			{
				BaseMeshShaders[MatIndex]->RecreateMaterialState();
			}

			if (MotionMeshShaders[MatIndex])
			{
				MotionMeshShaders[MatIndex]->RecreateMaterialState();
			}
		}
		else if (CompileFlag == UHMaterialCompileFlag::BindOnly)
		{
			if (DepthMeshShaders[MatIndex])
			{
				DepthMeshShaders[MatIndex]->BindParameters();
			}

			if (BaseMeshShaders[MatIndex])
			{
				BaseMeshShaders[MatIndex]->BindParameters();
			}

			if (MotionMeshShaders[MatIndex])
			{
				MotionMeshShaders[MatIndex]->BindParameters();
			}
		}
	}

	// recreate shader if need to assign renderer group again
	if (bNeedReassignRendererGroup || CompileFlag == UHMaterialCompileFlag::FullCompileResave || CompileFlag == UHMaterialCompileFlag::FullCompileTemporary)
	{
		Mat->UpdateMaterialUsage();
		for (UHObject* RendererObj : Mat->GetReferenceObjects())
		{
			if (UHMeshRendererComponent* Renderer = CastObject<UHMeshRendererComponent>(RendererObj))
			{
				RecreateMaterialShaders(Renderer, Mat);
			}
		}

		// mesh shader update if support
		if (GraphicInterface->IsMeshShaderSupported())
		{
			RecreateMeshShaders(Mat);
		}
	}

	// update hit group shader as well
	if (GraphicInterface->IsRayTracingEnabled() 
		&& CompileFlag != UHMaterialCompileFlag::BindOnly 
		&& CompileFlag != UHMaterialCompileFlag::StateChangedOnly
		&& !bDelayRTShaderCreation)
	{
		std::vector<UHMaterial*> Mats{ Mat };
		RecreateRTShaders(Mats, false);
	}

	Mat->SetCompileFlag(UHMaterialCompileFlag::UpToDate);

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
	if (OldMat == NewMat)
	{
		return;
	}

	// wait GPU finished
	GraphicInterface->WaitGPU();

	// remove old reference
	OldMat->RemoveReferenceObject(InRenderer);

	const bool bNeedReassignGroup = UHMaterial::IsDifferentBlendGroup(OldMat, NewMat);
	const bool bUseMeshShader = GraphicInterface->IsMeshShaderSupported();

	// set new material cache directly if it doesn't be reassigned
	// other wise remove the old renderer and recreate new one
	const int32_t RendererBufferIndex = InRenderer->GetBufferDataIndex();

	if (!bNeedReassignGroup)
	{
		if (NewMat->IsOpaque())
		{
			if (!bUseMeshShader)
			{
				if (GraphicInterface->IsDepthPrePassEnabled())
				{
					DepthPassShaders[RendererBufferIndex]->SetNewMaterialCache(NewMat);
				}
				BasePassShaders[RendererBufferIndex]->SetNewMaterialCache(NewMat);
				MotionOpaqueShaders[RendererBufferIndex]->SetNewMaterialCache(NewMat);
			}
		}
		else
		{
			if (!bUseMeshShader)
			{
				MotionTranslucentShaders[RendererBufferIndex]->SetNewMaterialCache(NewMat);
			}
			TranslucentPassShaders[RendererBufferIndex]->SetNewMaterialCache(NewMat);
		}

		// re-bind the parameter
		if (!bUseMeshShader)
		{
			ResetMaterialShaders(InRenderer, UHMaterialCompileFlag::BindOnly, NewMat->IsOpaque(), false);
		}
	}
	else
	{
		// state changed (different blendmode), need a recreate
		ResetMaterialShaders(InRenderer, UHMaterialCompileFlag::RendererMaterialChanged, OldMat->IsOpaque(), true);
		RecreateMaterialShaders(InRenderer, NewMat);
	}
	NewMat->AddReferenceObject(InRenderer);

	if (bUseMeshShader)
	{
		// both old & new mesh shader needs a update
		RecreateMeshShaders(OldMat);
		RecreateMeshShaders(NewMat);
	}

	UpdateDescriptors();
}

void UHDeferredShadingRenderer::ResetMaterialShaders(UHMeshRendererComponent* InMeshRenderer, UHMaterialCompileFlag CompileFlag, bool bIsOpaque, bool bNeedReassignRendererGroup)
{
	const int32_t RendererBufferIndex = InMeshRenderer->GetBufferDataIndex();
	const bool bReleaseDescriptorOnly = CompileFlag == UHMaterialCompileFlag::RendererMaterialChanged;
	const bool bEnableRayTracing = ConfigInterface->RenderingSetting().bEnableRayTracing && GraphicInterface->IsRayTracingEnabled();
	const bool bMeshShaderSupported = GraphicInterface->IsMeshShaderSupported();

	if (bNeedReassignRendererGroup 
		|| CompileFlag == UHMaterialCompileFlag::RendererMaterialChanged 
		|| CompileFlag == UHMaterialCompileFlag::FullCompileResave
		|| CompileFlag == UHMaterialCompileFlag::FullCompileTemporary)
	{
		// release shaders if it's re-compiling
		SafeReleaseShaderPtr(DepthPassShaders, RendererBufferIndex, bReleaseDescriptorOnly);
		SafeReleaseShaderPtr(BasePassShaders, RendererBufferIndex, bReleaseDescriptorOnly);
		SafeReleaseShaderPtr(MotionOpaqueShaders, RendererBufferIndex, bReleaseDescriptorOnly);
		SafeReleaseShaderPtr(MotionTranslucentShaders, RendererBufferIndex, bReleaseDescriptorOnly);
		SafeReleaseShaderPtr(TranslucentPassShaders, RendererBufferIndex, bReleaseDescriptorOnly);
	}
	else if (CompileFlag == UHMaterialCompileFlag::BindOnly && !bMeshShaderSupported)
	{
		// bind only
		if (bIsOpaque)
		{
			if (GraphicInterface->IsDepthPrePassEnabled())
			{
				DepthPassShaders[RendererBufferIndex]->BindParameters(InMeshRenderer);
			}

			BasePassShaders[RendererBufferIndex]->BindParameters(InMeshRenderer);
			MotionOpaqueShaders[RendererBufferIndex]->BindParameters(InMeshRenderer);
		}
		else
		{
			MotionTranslucentShaders[RendererBufferIndex]->BindParameters(InMeshRenderer);
			TranslucentPassShaders[RendererBufferIndex]->BindParameters(InMeshRenderer, bEnableRayTracing);
		}
	}
	else if (CompileFlag == UHMaterialCompileFlag::StateChangedOnly && !bMeshShaderSupported)
	{
		// re-create state only
		if (bIsOpaque)
		{
			if (GraphicInterface->IsDepthPrePassEnabled())
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
		const size_t RendererCount = CurrentScene->GetAllRendererCount();
		ObjectConstantsCPU.resize(RendererCount);
		for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
		{
			UH_SAFE_RELEASE(GObjectConstantBuffer[Idx]);
			GObjectConstantBuffer[Idx]->CreateBuffer(RendererCount, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		}
	}

	// do the same as initialization
	PrepareMeshes();
	PrepareTextures();

	for (UHMeshRendererComponent* Renderer : InRenderers)
	{
		UHMaterial* Mat = Renderer->GetMaterial();
		if (Mat)
		{
			RecreateMaterialShaders(Renderer, Mat);
		}
	}

	if (GraphicInterface->IsRayTracingEnabled())
	{
		RecreateRTShaders(std::vector<UHMaterial*>(), true);
	}
	UpdateDescriptors();
}

void UHDeferredShadingRenderer::ToggleDepthPrepass()
{
	// recreate pass obj/frame buffer
	std::vector<UHTexture*> GBufferTextures = { GSceneDiffuse ,GSceneNormal ,GSceneMaterial ,GSceneResult ,GSceneMip,GSceneVertexNormal };
	BasePassObj.Release(GraphicInterface->GetLogicalDevice());
	BasePassObj = GraphicInterface->CreateRenderPass(GBufferTextures, UHTransitionInfo(GraphicInterface->IsDepthPrePassEnabled() ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR)
		, GSceneDepth);

	std::vector<UHRenderTexture*> GBuffers;
	GBuffers.push_back(GSceneDiffuse);
	GBuffers.push_back(GSceneNormal);
	GBuffers.push_back(GSceneMaterial);
	GBuffers.push_back(GSceneResult);
	GBuffers.push_back(GSceneMip);
	GBuffers.push_back(GSceneVertexNormal);
	GBuffers.push_back(GSceneDepth);
	BasePassObj.FrameBuffer = GraphicInterface->CreateFrameBuffer(GBuffers, BasePassObj.RenderPass, RenderResolution);

	// recompile (trigger state recreation for shaders involved prepass flag)
	if (GraphicInterface->IsMeshShaderSupported())
	{
		for (auto& Shader : BaseMeshShaders)
		{
			if (Shader != nullptr)
			{
				Shader->SetNewRenderPass(BasePassObj.RenderPass);
				Shader->OnCompile();
			}
		}

		for (auto& Shader : MotionMeshShaders)
		{
			if (Shader->GetMaterialCache()->IsOpaque())
			{
				Shader->SetNewRenderPass(MotionOpaquePassObj.RenderPass);
				Shader->OnCompile();
			}
		}
	}
	else
	{
		for (auto& Shader : BasePassShaders)
		{
			Shader.second->SetNewRenderPass(BasePassObj.RenderPass);
			Shader.second->OnCompile();
		}

		for (auto& Shader : MotionOpaqueShaders)
		{
			Shader.second->SetNewRenderPass(MotionOpaquePassObj.RenderPass);
			Shader.second->OnCompile();
		}
	}
	UpdateDescriptors();
}

#endif

void UHDeferredShadingRenderer::RecreateMeshTables()
{
	if ((GraphicInterface->IsMeshShaderSupported() || GraphicInterface->IsRayTracingEnabled()) && MeshInstanceCount > 0)
	{
		UH_SAFE_RELEASE(PositionTable);
		UH_SAFE_RELEASE(UV0Table);
		UH_SAFE_RELEASE(NormalTable);
		UH_SAFE_RELEASE(TangentTable);
		UH_SAFE_RELEASE(IndicesTable);
		UH_SAFE_RELEASE(MeshletTable);

		PositionTable = MakeUnique<UHMeshTable>(GraphicInterface, "PositionTable", MeshInstanceCount);
		UV0Table = MakeUnique<UHMeshTable>(GraphicInterface, "UV0Table", MeshInstanceCount);
		NormalTable = MakeUnique<UHMeshTable>(GraphicInterface, "NormalTable", MeshInstanceCount);
		TangentTable = MakeUnique<UHMeshTable>(GraphicInterface, "TangentTable", MeshInstanceCount);
		IndicesTable = MakeUnique<UHMeshTable>(GraphicInterface, "IndicesTable", MeshInstanceCount);
		MeshletTable = MakeUnique<UHMeshTable>(GraphicInterface, "MeshletTable", MeshInstanceCount);
	}
}

void UHDeferredShadingRenderer::RecreateMaterialShaders(UHMeshRendererComponent* InMeshRenderer, UHMaterial* InMat)
{
	if (GraphicInterface->IsMeshShaderSupported() && InMat->IsOpaque())
	{
		// do not need material shaders for opaque
		return;
	}

	int32_t RendererBufferIndex = InMeshRenderer->GetBufferDataIndex();
	const std::vector<VkDescriptorSetLayout> BindlessLayouts = { TextureTable->GetDescriptorSetLayout(), SamplerTable->GetDescriptorSetLayout() };
	const bool bHasEnvCube = (GSkyLightCube != nullptr);

	if (InMat->IsOpaque())
	{
		if (GIsEditor || GraphicInterface->IsDepthPrePassEnabled())
		{
			DepthPassShaders[RendererBufferIndex] = MakeUnique<UHDepthPassShader>(GraphicInterface, "DepthPassShader", DepthPassObj.RenderPass, InMat, BindlessLayouts);
		}

		BasePassShaders[RendererBufferIndex] = MakeUnique<UHBasePassShader>(GraphicInterface, "BasePassShader", BasePassObj.RenderPass, InMat, BindlessLayouts);

		MotionOpaqueShaders[RendererBufferIndex] = MakeUnique<UHMotionObjectPassShader>(GraphicInterface, "MotionObjectShader", MotionOpaquePassObj.RenderPass, InMat, BindlessLayouts);
	}
	else
	{
		if (!GraphicInterface->IsMeshShaderSupported())
		{
			MotionTranslucentShaders[RendererBufferIndex] = MakeUnique<UHMotionObjectPassShader>(GraphicInterface, "MotionObjectShader", MotionTranslucentPassObj.RenderPass, InMat, BindlessLayouts);
		}
		TranslucentPassShaders[RendererBufferIndex]
			= MakeUnique<UHTranslucentPassShader>(GraphicInterface, "TranslucentPassShader", TranslucentPassObj.RenderPass, InMat, BindlessLayouts);
	}
}

void UHDeferredShadingRenderer::RecreateMeshShaders(UHMaterial* InMat)
{
	// only create when mesh shader is supported and opaque only
	if (!GraphicInterface->IsMeshShaderSupported() || MeshInstanceCount == 0)
	{
		return;
	}

	std::vector<VkDescriptorSetLayout> BindlessLayouts = { TextureTable->GetDescriptorSetLayout()
		, SamplerTable->GetDescriptorSetLayout()
		, MeshletTable->GetDescriptorSetLayout()
		, PositionTable->GetDescriptorSetLayout()
		, UV0Table->GetDescriptorSetLayout()
		, IndicesTable->GetDescriptorSetLayout()
	};

	const uint32_t MatDataIndex = InMat->GetBufferDataIndex();
	if (MatDataIndex >= CurrentScene->GetMaterialCount())
	{
		return;
	}

	// create depth and base shader only for the opaque, but motion pass for both opaque/translucent
	if (InMat->IsOpaque())
	{
		if (GIsEditor || ConfigInterface->RenderingSetting().bEnableDepthPrePass)
		{
			UH_SAFE_RELEASE(DepthMeshShaders[MatDataIndex]);
			DepthMeshShaders[MatDataIndex] = MakeUnique<UHDepthMeshShader>(GraphicInterface, "DepthMeshShader", DepthPassObj.RenderPass, InMat, BindlessLayouts);
		}

		// push normal and tangent table after depth
		BindlessLayouts.push_back(NormalTable->GetDescriptorSetLayout());
		BindlessLayouts.push_back(TangentTable->GetDescriptorSetLayout());

		UH_SAFE_RELEASE(BaseMeshShaders[MatDataIndex]);
		BaseMeshShaders[MatDataIndex] = MakeUnique<UHBaseMeshShader>(GraphicInterface, "BaseMeshShader", BasePassObj.RenderPass, InMat, BindlessLayouts);
	}
	else
	{
		BindlessLayouts.push_back(NormalTable->GetDescriptorSetLayout());
		BindlessLayouts.push_back(TangentTable->GetDescriptorSetLayout());
	}

	UH_SAFE_RELEASE(MotionMeshShaders[MatDataIndex]);
	MotionMeshShaders[MatDataIndex] = MakeUnique<UHMotionMeshShader>(GraphicInterface, "MotionMeshShader"
		, InMat->IsOpaque() ? MotionOpaquePassObj.RenderPass : MotionTranslucentPassObj.RenderPass, InMat, BindlessLayouts);

	// count total meshlet number of a material group
	uint32_t MeshletCountOfMaterialGroup = 0;

	const std::vector<UHObject*>& Objects = InMat->GetReferenceObjects();
	for (UHObject* Obj : Objects)
	{
		if (const UHMeshRendererComponent* Renderer = CastObject<UHMeshRendererComponent>(Obj))
		{
			if (const UHMesh* Mesh = Renderer->GetMesh())
			{
				MeshletCountOfMaterialGroup += Mesh->GetMeshletCount();
			}
		}
	}

	// create mesh shader data for this material group
	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		UH_SAFE_RELEASE(GMeshShaderData[Idx][MatDataIndex]);
		GMeshShaderData[Idx][MatDataIndex] = GraphicInterface->RequestRenderBuffer<UHMeshShaderData>(MeshletCountOfMaterialGroup, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
			, "MeshShaderData");

		UH_SAFE_RELEASE(GMotionOpaqueShaderData[Idx][MatDataIndex]);
		GMotionOpaqueShaderData[Idx][MatDataIndex] = GraphicInterface->RequestRenderBuffer<UHMeshShaderData>(MeshletCountOfMaterialGroup, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
			, "MotionOpaqueShaderData");

		UH_SAFE_RELEASE(GMotionTranslucentShaderData[Idx][MatDataIndex]);
		GMotionTranslucentShaderData[Idx][MatDataIndex] = GraphicInterface->RequestRenderBuffer<UHMeshShaderData>(MeshletCountOfMaterialGroup, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
			, "MotionTranslucentShaderData");
	}

	MeshShaderInstancesCounter[MatDataIndex] = 0;
	VisibleMeshShaderData[MatDataIndex].reserve(MeshletCountOfMaterialGroup);
	MotionOpaqueMeshShaderData[MatDataIndex].reserve(MeshletCountOfMaterialGroup);
	MotionTranslucentMeshShaderData[MatDataIndex].reserve(MeshletCountOfMaterialGroup);
}

void UHDeferredShadingRenderer::RecreateRTShaders(std::vector<UHMaterial*> InMats, bool bRecreateTable)
{
	if (!ConfigInterface->RenderingSetting().bEnableRayTracing)
	{
		return;
	}

	if (bRecreateTable)
	{
		// buffer for storing index type, used in hit group shader
		UH_SAFE_RELEASE(RTMeshInstanceTable);
		RTMeshInstanceTable = MakeUnique<UHRTMeshInstanceTable>(GraphicInterface, "RTMeshInstanceTable");

		// buffer for storing texture index
		UH_SAFE_RELEASE(RTMaterialDataTable);
		RTMaterialDataTable = MakeUnique<UHRTMaterialDataTable>(GraphicInterface, "RTMaterialDataTable", static_cast<uint32_t>(CurrentScene->GetMaterialCount()));

		UH_SAFE_RELEASE(RTTextureTable);
		RTTextureTable = MakeUnique<UHRTTextureTable>(GraphicInterface, "RTTextureTable");

		UH_SAFE_RELEASE(RTDefaultHitGroupShader);
		RTDefaultHitGroupShader = MakeUnique<UHRTDefaultHitGroupShader>(GraphicInterface, "RTDefaultHitGroupShader", CurrentScene->GetMaterials());
	}

	for (size_t Idx = 0; Idx < InMats.size(); Idx++)
	{
		if (InMats[Idx])
		{
			RTDefaultHitGroupShader->UpdateHitShader(GraphicInterface, InMats[Idx]);
		}
	}

	const std::vector<VkDescriptorSetLayout> Layouts = { TextureTable->GetDescriptorSetLayout()
		, SamplerTable->GetDescriptorSetLayout()
		, RTMeshInstanceTable->GetDescriptorSetLayout()
		, UV0Table->GetDescriptorSetLayout()
		, NormalTable->GetDescriptorSetLayout()
		, TangentTable->GetDescriptorSetLayout()
		, IndicesTable->GetDescriptorSetLayout()
		, RTMaterialDataTable->GetDescriptorSetLayout()
		, RTTextureTable->GetDescriptorSetLayout() };

	UH_SAFE_RELEASE(RTShadowShader);
	UH_SAFE_RELEASE(RTReflectionShader);

	RTShadowShader = MakeUnique<UHRTShadowShader>(GraphicInterface, "RTShadowShader", RTDefaultHitGroupShader->GetClosestShaders(), RTDefaultHitGroupShader->GetAnyHitShaders()
		, Layouts);

	RTReflectionShader = MakeUnique<UHRTReflectionShader>(GraphicInterface, "RTReflectionShader", RTDefaultHitGroupShader->GetClosestShaders(), RTDefaultHitGroupShader->GetAnyHitShaders()
		, Layouts);

	// setup RT descriptor sets that will be used in rendering
	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		RTDescriptorSets[Idx].clear();
		RTDescriptorSets[Idx] = { nullptr
			, TextureTable->GetDescriptorSet(Idx)
			, SamplerTable->GetDescriptorSet(Idx)
			, RTMeshInstanceTable->GetDescriptorSet(Idx)
			, UV0Table->GetDescriptorSet(Idx)
			, NormalTable->GetDescriptorSet(Idx)
			, TangentTable->GetDescriptorSet(Idx)
			, IndicesTable->GetDescriptorSet(Idx)
			, RTMaterialDataTable->GetDescriptorSet(Idx)
			, RTTextureTable->GetDescriptorSet(Idx) };
	}
}