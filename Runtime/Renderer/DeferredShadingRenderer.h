#pragma once
#include "../../framework.h"
#include "../../UnheardEngine.h"
#include "../Engine/Graphic.h"
#include "../Engine/Asset.h"
#include "../Engine/Config.h"
#include "../Classes/Shader.h"
#include "../Classes/Scene.h"
#include "../Classes/GraphicState.h"
#include "../Classes/Sampler.h"
#include "../Classes/GPUQuery.h"
#include "../Classes/Thread.h"
#include "RenderingTypes.h"
#include "RendererShared.h"
#include "RenderBuilder.h"
#include "ParallelSubmitter.h"
#include "QueueSubmitter.h"
#include <memory>
#include <unordered_map>

// shader includes
#include "ShaderClass/DepthPassShader.h"
#include "ShaderClass/BasePassShader.h"
#include "ShaderClass/LightCullingShader.h"
#include "ShaderClass/LightPassShader.h"
#include "ShaderClass/SkyPassShader.h"
#include "ShaderClass/MotionPassShader.h"
#include "ShaderClass/TranslucentPassShader.h"
#include "ShaderClass/PostProcessing/ToneMappingShader.h"
#include "ShaderClass/PostProcessing/TemporalAAShader.h"
#include "ShaderClass/PostProcessing/GaussianFilterShader.h"
#include "ShaderClass/RayTracing/RTDefaultHitGroupShader.h"
#include "ShaderClass/RayTracing/RTShadowShader.h"
#include "ShaderClass/RayTracing/RTReflectionShader.h"
#include "ShaderClass/TextureSamplerTable.h"
#include "ShaderClass/MeshTable.h"
#include "ShaderClass/RayTracing/RTMaterialDataTable.h"
#include "ShaderClass/RayTracing/SoftRTShadowShader.h"
#include "ShaderClass/SphericalHarmonicShader.h"
#include "ShaderClass/RayTracing/RTTextureTable.h"
#include "ShaderClass/ReflectionPassShader.h"
#include "ShaderClass/RayTracing/RTReflectionMipmap.h"
#include "ShaderClass/RayTracing/RTMeshInstanceTable.h"
#include "ShaderClass/OcclusionPassShader.h"

#if WITH_EDITOR
#include "ShaderClass/PostProcessing/DebugViewShader.h"
#include "ShaderClass/PostProcessing/DebugBoundShader.h"
#endif
#include "../../Editor/Editor/Profiler.h"

enum class UHParallelTask
{
	None = -1,
	FrustumCullingTask,
	DepthPassTask,
	OcclusionPassTask,
	BasePassTask,
	MotionOpaqueTask,
	MotionTranslucentTask,
	TranslucentBgPassTask
};

// Deferred Shading Renderer class for Unheard Engine, initialize with a UHGraphic pointer and a asset pointer
class UHDeferredShadingRenderer
{
public:
	UHDeferredShadingRenderer(UHEngine* InEngine);
	bool Initialize(UHScene* InScene);
	void InitRenderingResources();
	void Release();

	UHScene* GetCurrentScene() const;
	void SetSwapChainReset(bool bInFlag);
	bool IsNeedReset();

	void Resize();
	void Reset();
	void Update();
	void NotifyRenderThread();
	void WaitPreviousRenderTask();

	// only resize RT buffers
	void ReleaseRayTracingBuffers();
	void ResizeRayTracingBuffers(bool bInitOnly);

	// update descriptors
	void UpdateDescriptors();
	void UpdateTextureDescriptors();

#if WITH_EDITOR
	void SetDebugViewIndex(int32_t Idx);
	void SetEditorDelta(uint32_t InWidthDelta, uint32_t InHeightDelta);

	float GetRenderThreadTime() const;
	int32_t GetDrawCallCount() const;
	int32_t GetOccludedCallCount() const;

	static UHDeferredShadingRenderer* GetRendererEditorOnly();
	void RefreshSkyLight(bool bNeedRecompile);
	void RefreshMaterialShaders(UHMaterial* InMat, bool bNeedReassignRendererGroup, bool bDelayRTShaderCreation);
	void OnRendererMaterialChanged(UHMeshRendererComponent* InRenderer, UHMaterial* OldMat, UHMaterial* NewMat);

	void ResetMaterialShaders(UHMeshRendererComponent* InMeshRenderer, UHMaterialCompileFlag CompileFlag, bool bIsOpaque, bool bNeedReassignRendererGroup);
	void AppendMeshRenderers(const std::vector<UHMeshRendererComponent*> InRenderers);

	void ToggleDepthPrepass();
#endif
	void RecreateMeshTables();
	void RecreateMaterialShaders(UHMeshRendererComponent* InMeshRenderer, UHMaterial* InMat);
	void RecreateMeshShaders(UHMaterial* InMat);
	void RecreateRTShaders(std::vector<UHMaterial*> InMats, bool bRecreateTable);

	void CalculateBlurWeights(const int32_t InRadius, float* OutWeights);
	bool DispatchGaussianFilter(UHRenderBuilder& RenderBuilder, const std::string& InName
		, UHTexture* Input, UHRenderTexture* Output
		, const UHGaussianFilterConstants& Constants);

	// occlusion query
	void CreateOcclusionQuery();
	void ReleaseOcclusionQuery();

	// async compute queue
	bool CreateAsyncComputeQueue();
	void ReleaseAsyncComputeQueue();

	/************************************************ parallel task functions ************************************************/
	// based on the scatter and gather method
	void DepthPassTask(int32_t ThreadIdx);
	void OcclusionPassTask(int32_t ThreadIdx);
	void BasePassTask(int32_t ThreadIdx);
	void MotionOpaqueTask(int32_t ThreadIdx);
	void MotionTranslucentTask(int32_t ThreadIdx);
	void TranslucentPassTask(int32_t ThreadIdx);

private:
	/************************************************ functions ************************************************/
	void RenderThreadLoop();
	void WorkerThreadLoop(int32_t ThreadIdx);

	// prepare meshes
	void PrepareMeshes();

	// prepare textures
	void CheckTextureReference(std::vector<UHMaterial*> InMats);
	void PrepareTextures();

	// prepare samplers
	void PrepareSamplers();

	// prepare rendering shaders
	void PrepareRenderingShaders();

	// init queue submitters
	bool InitQueueSubmitters();

	// release shaders
	void ReleaseShaders();

	// create rendering buffers that will be used
	void CreateRenderingBuffers();

	// destroy rendering buffers
	void RelaseRenderingBuffers();

	// create render passes
	void CreateRenderPasses();
	void CreateRenderFrameBuffers();

	// release render pass objects
	void ReleaseRenderPassObjects();

	// create constant buffers
	void CreateDataBuffers();

	// create thread objects
	void CreateThreadObjects();

	// release constant buffers
	void ReleaseDataBuffers();

	// upload data buffers
	void UploadDataBuffers();

	// frustum culling
	void FrustumCulling();

	// collect visible renderer
	void CollectVisibleRenderer();

	// collect mesh shader instance
	void CollectMeshShaderInstance();

	// get light culling tile count
	void GetLightCullingTileCount(uint32_t& TileCountX, uint32_t& TileCountY);

	// get current skycube
	UHTextureCube* GetCurrentSkyCube() const;

	void InitGaussianConstants();


	/************************************************ rendering functions ************************************************/
	void BuildTopLevelAS(UHRenderBuilder& RenderBuilder);
	void ResolveOcclusionResult(UHRenderBuilder& RenderBuilder);
	void RenderDepthPrePass(UHRenderBuilder& RenderBuilder);
	void RenderOcclusionPass(UHRenderBuilder& RenderBuilder);
	void RenderBasePass(UHRenderBuilder& RenderBuilder);
	void DispatchLightCulling(UHRenderBuilder& RenderBuilder);
	void DispatchRayShadowPass(UHRenderBuilder& RenderBuilder);
	void DispatchRayReflectionPass(UHRenderBuilder& RenderBuilder);
	void RenderLightPass(UHRenderBuilder& RenderBuilder);
	void PreReflectionPass(UHRenderBuilder& RenderBuilder);
	void DrawReflectionPass(UHRenderBuilder& RenderBuilder);
	void GenerateSH9Pass(UHRenderBuilder& RenderBuilder);
	void RenderSkyPass(UHRenderBuilder& RenderBuilder);
	void RenderMotionPass(UHRenderBuilder& RenderBuilder);
	void RenderTranslucentPass(UHRenderBuilder& RenderBuilder);
	void RenderEffect(UHShaderClass* InShader, UHRenderBuilder& RenderBuilder, int32_t& PostProcessIdx, std::string InName);
	void Dispatch2DEffect(UHShaderClass* InShader, UHRenderBuilder& RenderBuilder, int32_t& PostProcessIdx, std::string InName);
	void RenderPostProcessing(UHRenderBuilder& RenderBuilder);

	void ScreenshotForRefraction(std::string PassName, UHRenderBuilder& RenderBuilder, UHGaussianFilterConstants Constants);

	uint32_t RenderSceneToSwapChain(UHRenderBuilder& RenderBuilder);

#if WITH_EDITOR
	void RenderComponentBounds(UHRenderBuilder& RenderBuilder, const int32_t PostProcessIdx);
#endif

	/************************************************ variables ************************************************/
	UHGraphic* GraphicInterface;
	UHAssetManager* AssetManagerInterface;
	UHConfigManager* ConfigInterface;
	UHGameTimer* TimerInterface;
	VkExtent2D RenderResolution;
	VkExtent2D RTShadowExtent;

	// queue submitter
	bool bEnableAsyncComputeRT;
	UHQueueSubmitter AsyncComputeQueue;
	UHQueueSubmitter SceneRenderQueue;

	// parallel submitters
	UHParallelSubmitter DepthParallelSubmitter;
	UHParallelSubmitter OcclusionParallelSubmitter;
	UHParallelSubmitter BaseParallelSubmitter;
	UHParallelSubmitter MotionOpaqueParallelSubmitter;
	UHParallelSubmitter MotionTranslucentParallelSubmitter;
	UHParallelSubmitter TranslucentParallelSubmitter;

	// current frame count
	uint32_t CurrentFrameGT;
	uint32_t CurrentFrameRT;
	uint32_t FrameNumberRT;

	// Render thread defines, UH engine will always use a thread for rendering, and doing parallel submission with worker threads
	UniquePtr<UHThread> RenderThread;
	int32_t NumWorkerThreads;
	std::vector<UniquePtr<UHThread>> WorkerThreads;
	bool bIsResetNeededShared;
	bool bVsyncRT;
	bool bIsSwapChainResetGT;
	bool bIsSwapChainResetRT;
	bool bIsRenderingEnabledRT;
	bool bIsSkyLightEnabledRT;
	bool bNeedGenerateSH9RT;
	bool bHasRefractionMaterialGT;
	bool bHasRefractionMaterialRT;
	bool bHDREnabledRT;
	bool bEnableHWOcclusionRT;
	bool bEnableDepthPrepassRT;
	int32_t OcclusionThresholdRT;
	float RTCullingDistanceRT;
	int32_t RTReflectionQualityRT;

	// current scene
	UHScene* CurrentScene;

	// system constant
	UHSystemConstants SystemConstantsCPU;

	// object & material constants, I'll create constant buffer which are big enough for all renderers
	std::vector<UHObjectConstants> ObjectConstantsCPU;
	std::vector<UHObjectConstants> OcclusionConstantsCPU;

	// light buffers, this will be used as structure buffer instead of constant
	std::vector<UHDirectionalLightConstants> DirLightConstantsCPU;
	std::vector<UHPointLightConstants> PointLightConstantsCPU;
	std::vector<UHSpotLightConstants> SpotLightConstantsCPU;

	// shared samplers
	int32_t DefaultSamplerIndex;
	int32_t LinearClampSamplerIndex;
	int32_t PointClampSamplerIndex;
	int32_t SkyCubeSamplerIndex;
	int32_t RefractionClearIndex;
	int32_t RefractionBlurredIndex;

	// bindless table
	UniquePtr<UHTextureTable> TextureTable;
	UniquePtr<UHSamplerTable> SamplerTable;


	/************************************************ Render Pass stuffs ************************************************/

	// -------------------------------------------- Depth Pass -------------------------------------------- //
	std::unordered_map<int32_t, UniquePtr<UHDepthPassShader>> DepthPassShaders;
	UHRenderPassObject DepthPassObj;

	// -------------------------------------------- Base Pass -------------------------------------------- //
	// store different base pass object, the id is renderer data index
	std::unordered_map<int32_t, UniquePtr<UHBasePassShader>> BasePassShaders;
	UHRenderPassObject BasePassObj;

	// -------------------------------------------- Light and Light Culling Pass -------------------------------------------- //
	const uint32_t LightCullingTileSize;
	const uint32_t MaxPointLightPerTile;
	const uint32_t MaxSpotLightPerTile;
	UniquePtr<UHLightCullingShader> LightCullingShader;
	UniquePtr<UHLightPassShader> LightPassShader;
	UniquePtr<UHReflectionPassShader> ReflectionPassShader;
	UniquePtr<UHRTReflectionMipmap> RTReflectionMipmapShader;

	// -------------------------------------------- Skybox Pass -------------------------------------------- //
	UniquePtr<UHSkyPassShader> SkyPassShader;
	UniquePtr<UHSphericalHarmonicShader> SH9Shader;

	UHRenderPassObject SkyboxPassObj;
	UHMesh* CubeMesh;

	// -------------------------------------------- Motion vector Pass -------------------------------------------- //
	UHRenderPassObject MotionCameraPassObj;
	UniquePtr<UHMotionCameraPassShader> MotionCameraShader;

	// store different motion pass object, thd id is buffer data index(per renderer)
	// the motion shader is separate into opaque and translucent
	UHRenderPassObject MotionOpaquePassObj;
	std::unordered_map<int32_t, UniquePtr<UHMotionObjectPassShader>> MotionOpaqueShaders;
	UHRenderPassObject MotionTranslucentPassObj;
	std::unordered_map<int32_t, UniquePtr<UHMotionObjectPassShader>> MotionTranslucentShaders;

	// -------------------------------------------- Translucent Pass -------------------------------------------- //
	std::unordered_map<int32_t, UniquePtr<UHTranslucentPassShader>> TranslucentPassShaders;
	UHRenderPassObject TranslucentPassObj;

	// -------------------------------------------- Post processing Pass -------------------------------------------- //
	// post process needs to use two textures and keep blit to each other, so the effects can be accumulated
	static const int32_t NumOfPostProcessRT = 2;
	int32_t PostProcessResultIdx;
	UHRenderPassObject PostProcessPassObj[NumOfPostProcessRT];
	UHRenderTexture* PostProcessResults[NumOfPostProcessRT];

	UniquePtr<UHToneMappingShader> ToneMapShader;
	UniquePtr<UHTemporalAAShader> TemporalAAShader;
	UniquePtr<UHGaussianFilterShader> GaussianFilterHShader;
	UniquePtr<UHGaussianFilterShader> GaussianFilterVShader;
	bool bIsTemporalReset;

	// gaussian constants
	UHGaussianFilterConstants RayTracingGaussianConsts;
	UHGaussianFilterConstants RefractionGaussianConsts;

#if WITH_EDITOR
	// debug view shader
	UniquePtr<UHDebugViewShader> DebugViewShader;
	int32_t DebugViewIndex;

	// debug bound shader
	UniquePtr<UHDebugBoundShader> DebugBoundShader;

	// profiles
	float RenderThreadTime;
	int32_t DrawCalls;
	int32_t OccludedCalls;
	std::vector<int32_t> ThreadDrawCalls;
	std::vector<int32_t> ThreadOccludedCalls;

	// GUI
	uint32_t EditorWidthDelta;
	uint32_t EditorHeightDelta;

	static UHDeferredShadingRenderer* SceneRendererEditorOnly;
	bool bDrawDebugViewRT;
#endif
	UHGPUQuery* GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::UHRenderPassMax)];

	// -------------------------------------------- Ray tracing related -------------------------------------------- //
	UniquePtr<UHRTDefaultHitGroupShader> RTDefaultHitGroupShader;
	UniquePtr<UHSoftRTShadowShader> SoftRTShadowShader;
	UniquePtr<UHRTShadowShader> RTShadowShader;
	UniquePtr<UHRTReflectionShader> RTReflectionShader;
	std::vector<VkDescriptorSet> RTDescriptorSets[GMaxFrameInFlight];

	UniquePtr<UHRTMeshInstanceTable> RTMeshInstanceTable;
	UniquePtr<UHRTMaterialDataTable> RTMaterialDataTable;
	UniquePtr<UHRTTextureTable> RTTextureTable;

	uint32_t RTInstanceCount;
	bool bIsRaytracingEnableRT;

	// -------------------------------------------- Culling & sorting related -------------------------------------------- //
	std::vector<UHMeshRendererComponent*> OpaquesToRender;
	std::vector<UHMeshRendererComponent*> MotionOpaquesToRender;
	std::vector<UHMeshRendererComponent*> TranslucentsToRender;
	std::vector<UHMeshRendererComponent*> OcclusionRenderers;

	// max element setting for counting sort, higher number = better result for GPU time but longer CPU time
	// lower = better CPU time but longer GPU time, need to trade off between these.
	static const int32_t MaxCountingElement = 4096;
	std::vector<UHMeshRendererComponent*> CountingRenderers[MaxCountingElement];

	UHGPUQuery* OcclusionQuery[GMaxFrameInFlight];
	std::vector<UniquePtr<UHOcclusionPassShader>> OcclusionPassShaders;
	UHRenderPassObject OcclusionPassObj;

	// -------------------------------------------- Mesh shader related -------------------------------------------- //
	UniquePtr<UHMeshTable> PositionTable;
	UniquePtr<UHMeshTable> UV0Table;
	UniquePtr<UHMeshTable> NormalTable;
	UniquePtr<UHMeshTable> TangentTable;
	UniquePtr<UHMeshTable> IndicesTable;
	UniquePtr<UHMeshTable> MeshletTable;

	uint32_t MeshInstanceCount;
	std::vector<UHMesh*> MeshInUse;

	// access following data with material's buffer data index
	std::vector<int32_t> MeshShaderInstancesCounter;
	std::vector<int32_t> SortedMeshShaderGroupIndex;
	std::vector<std::vector<UHMeshShaderData>> VisibleMeshShaderData;

	// motion mesh shader needs another mesh shader data list, as not all visible meshes need to output vector every frame
	std::vector<std::vector<UHMeshShaderData>> MotionOpaqueMeshShaderData;
	std::vector<std::vector<UHMeshShaderData>> MotionTranslucentMeshShaderData;

	std::vector<UniquePtr<UHDepthMeshShader>> DepthMeshShaders;
	std::vector<UniquePtr<UHBaseMeshShader>> BaseMeshShaders;
	std::vector<UniquePtr<UHMotionMeshShader>> MotionMeshShaders;
};