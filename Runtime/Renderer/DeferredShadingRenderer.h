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
#include "ShaderClass/RayTracing/RTDefaultHitGroupShader.h"
#include "ShaderClass/RayTracing/RTShadowShader.h"
#include "ShaderClass/TextureSamplerTable.h"
#include "ShaderClass/RayTracing/RTMeshTable.h"
#include "ShaderClass/RayTracing/RTMaterialDataTable.h"
#include "ShaderClass/RayTracing/SoftRTShadowShader.h"
#include "ShaderClass/SphericalHarmonicShader.h"

#if WITH_EDITOR
#include "ShaderClass/PostProcessing/DebugViewShader.h"
#include "ShaderClass/PostProcessing/DebugBoundShader.h"
#endif
#include "../../Editor/Editor/Profiler.h"

enum UHParallelTask
{
	None = -1,
	FrustumCullingTask,
	SortingOpaqueTask,
	DepthPassTask,
	BasePassTask,
	MotionOpaqueTask,
	MotionTranslucentTask,
	TranslucentPassTask
};

// Deferred Shading Renderer class for Unheard Engine, initialize with a UHGraphic pointer and a asset pointer
class UHDeferredShadingRenderer
{
public:
	UHDeferredShadingRenderer(UHGraphic* InGraphic, UHAssetManager* InAssetManager, UHConfigManager* InConfig, UHGameTimer* InTimer);
	bool Initialize(UHScene* InScene);
	void Release();

	void SetCurrentScene(UHScene* InScene);
	UHScene* GetCurrentScene() const;
	void SetSwapChainReset(bool bInFlag);
	bool IsNeedReset();

	void Resize();
	void Reset();
	void Update();
	void NotifyRenderThread();
	void WaitPreviousRenderTask();

	// only resize RT buffers
	void ResizeRayTracingBuffers();
	void UpdateTextureDescriptors();

#if WITH_EDITOR
	void SetDebugViewIndex(int32_t Idx);
	void SetEditorDelta(uint32_t InWidthDelta, uint32_t InHeightDelta);

	float GetRenderThreadTime() const;
	int32_t GetDrawCallCount() const;
	float* GetGPUTimes();

	static UHDeferredShadingRenderer* GetRendererEditorOnly();
	void RefreshSkyLight(bool bNeedRecompile);
	void RefreshMaterialShaders(UHMaterial* InMat, bool bNeedReassignRendererGroup = false);
	void OnRendererMaterialChanged(UHMeshRendererComponent* InRenderer, UHMaterial* OldMat, UHMaterial* NewMat);

	void ResetMaterialShaders(UHMeshRendererComponent* InMeshRenderer, UHMaterialCompileFlag CompileFlag, bool bIsOpaque, bool bNeedReassignRendererGroup);
	void AppendMeshRenderers(const std::vector<UHMeshRendererComponent*> InRenderers);
#endif
	void RecreateMaterialShaders(UHMeshRendererComponent* InMeshRenderer, UHMaterial* InMat);
	void RecreateRTShaders(UHMaterial* InMat, bool bRecreateTable);

private:
	/************************************************ functions ************************************************/
	void RenderThreadLoop();
	void WorkerThreadLoop(int32_t ThreadIdx);

	// prepare meshes
	void PrepareMeshes();

	// prepare textures
	void CheckTextureReference(UHMaterial* InMat);
	void PrepareTextures();

	// prepare samplers
	void PrepareSamplers();

	// prepare rendering shaders
	void PrepareRenderingShaders();

	// init queue submitters
	bool InitQueueSubmitters();

	// update descriptors
	void UpdateDescriptors();

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
	void ReleaseRenderPassObjects(bool bFrameBufferOnly = false);

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

	// sort renderer
	void SortRenderer();

	// get light culling tile count
	void GetLightCullingTileCount(uint32_t& TileCountX, uint32_t& TileCountY);

	// get current skycube
	UHTextureCube* GetCurrentSkyCube() const;


	/************************************************ rendering functions ************************************************/
	void BuildTopLevelAS(UHRenderBuilder& RenderBuilder);
	void RenderDepthPrePass(UHRenderBuilder& RenderBuilder);
	void RenderBasePass(UHRenderBuilder& RenderBuilder);
	void DispatchLightCulling(UHRenderBuilder& RenderBuilder);
	void DispatchRayShadowPass(UHRenderBuilder& RenderBuilder);
	void RenderLightPass(UHRenderBuilder& RenderBuilder);
	void GenerateSH9Pass(UHRenderBuilder& RenderBuilder);
	void RenderSkyPass(UHRenderBuilder& RenderBuilder);
	void RenderMotionPass(UHRenderBuilder& RenderBuilder);
	void RenderTranslucentPass(UHRenderBuilder& RenderBuilder);
	void RenderEffect(UHShaderClass* InShader, UHRenderBuilder& RenderBuilder, int32_t& PostProcessIdx, std::string InName);
	void DispatchEffect(UHShaderClass* InShader, UHRenderBuilder& RenderBuilder, int32_t& PostProcessIdx, std::string InName);
	void RenderPostProcessing(UHRenderBuilder& RenderBuilder);
	uint32_t RenderSceneToSwapChain(UHRenderBuilder& RenderBuilder);

#if WITH_EDITOR
	void RenderComponentBounds(UHRenderBuilder& RenderBuilder, const int32_t PostProcessIdx);
#endif


	/************************************************ parallel task functions ************************************************/
	// based on the scatter and gather method
	void FrustumCullingTask(int32_t ThreadIdx);
	void SortingOpaqueTask(int32_t ThreadIdx);
	void DepthPassTask(int32_t ThreadIdx);
	void BasePassTask(int32_t ThreadIdx);
	void MotionOpaqueTask(int32_t ThreadIdx);
	void MotionTranslucentTask(int32_t ThreadIdx);
	void TranslucentPassTask(int32_t ThreadIdx);


	/************************************************ variables ************************************************/

	UHGraphic* GraphicInterface;
	UHAssetManager* AssetManagerInterface;
	UHConfigManager* ConfigInterface;
	UHGameTimer* TimerInterface;
	VkExtent2D RenderResolution;
	VkExtent2D RTShadowExtent;

	// queue submitter
	bool bEnableAsyncComputeGT;
	bool bEnableAsyncComputeRT;
	UHQueueSubmitter AsyncComputeQueue;
	UHQueueSubmitter SceneRenderQueue;

	// parallel submitters
	UHParallelSubmitter DepthParallelSubmitter;
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
	UHParallelTask ParallelTask;
	bool bParallelSubmissionRT;
	bool bVsyncRT;
	bool bIsSwapChainResetGT;
	bool bIsSwapChainResetRT;
	bool bIsRenderingEnabledRT;
	bool bIsSkyLightEnabledRT;
	bool bNeedGenerateSH9RT;

	// current scene
	UHScene* CurrentScene;

	// system constant
	UHSystemConstants SystemConstantsCPU;

	// object & material constants, I'll create constant buffer which are big enough for all renderers
	std::vector<UHObjectConstants> ObjectConstantsCPU;

	// light buffers, this will be used as structure buffer instead of constant
	std::vector<UHDirectionalLightConstants> DirLightConstantsCPU;
	std::vector<UHPointLightConstants> PointLightConstantsCPU;
	std::vector<UHSpotLightConstants> SpotLightConstantsCPU;

	// shared samplers
	int32_t DefaultSamplerIndex;

	// bindless table
	UniquePtr<UHTextureTable> TextureTable;
	UniquePtr<UHSamplerTable> SamplerTable;


	/************************************************ Render Pass stuffs ************************************************/

	// -------------------------------------------- Depth Pass -------------------------------------------- //
	std::unordered_map<int32_t, UniquePtr<UHDepthPassShader>> DepthPassShaders;
	UHRenderPassObject DepthPassObj;
	bool bEnableDepthPrePass;

	// -------------------------------------------- Base Pass -------------------------------------------- //

	// GBuffers
	UHTextureFormat DiffuseFormat;
	UHTextureFormat NormalFormat;
	UHTextureFormat SpecularFormat;
	UHTextureFormat SceneResultFormat;
	UHTextureFormat SceneMipFormat;
	UHTextureFormat DepthFormat;
	UHTextureFormat HDRFormat;

	// store different base pass object, the id is renderer data index
	std::unordered_map<int32_t, UniquePtr<UHBasePassShader>> BasePassShaders;
	UHRenderPassObject BasePassObj;

	// -------------------------------------------- Light and Light Culling Pass -------------------------------------------- //
	const uint32_t LightCullingTileSize;
	const uint32_t MaxPointLightPerTile;
	const uint32_t MaxSpotLightPerTile;
	UniquePtr<UHLightCullingShader> LightCullingShader;
	UniquePtr<UHLightPassShader> LightPassShader;

	// -------------------------------------------- Skybox Pass -------------------------------------------- //
	UniquePtr<UHSkyPassShader> SkyPassShader;
	UniquePtr<UHSphericalHarmonicShader> SH9Shader;

	UHRenderPassObject SkyboxPassObj;
	UHMesh* SkyMeshRT;

	// -------------------------------------------- Motion vector Pass -------------------------------------------- //
	UHTextureFormat MotionFormat;
	UHRenderPassObject MotionCameraPassObj;
	UniquePtr<UHMotionCameraPassShader> MotionCameraShader;
	UniquePtr<UHMotionCameraPassShader> MotionCameraWorkaroundShader;

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
	bool bIsTemporalReset;

#if WITH_EDITOR
	// debug view shader
	UniquePtr<UHDebugViewShader> DebugViewShader;
	int32_t DebugViewIndex;

	// debug bound shader
	UniquePtr<UHDebugBoundShader> DebugBoundShader;

	// profiles
	float RenderThreadTime;
	int32_t DrawCalls;
	std::vector<int32_t> ThreadDrawCalls;
	float GPUTimes[UHRenderPassTypes::UHRenderPassMax];

	// GUI
	uint32_t EditorWidthDelta;
	uint32_t EditorHeightDelta;

	static UHDeferredShadingRenderer* SceneRendererEditorOnly;
	bool bDrawDebugViewRT;
#endif
	UHGPUQuery* GPUTimeQueries[UHRenderPassTypes::UHRenderPassMax];

	// -------------------------------------------- Ray tracing related -------------------------------------------- //
	UHTextureFormat RTShadowFormat;
	UniquePtr<UHAccelerationStructure> TopLevelAS[GMaxFrameInFlight];

	UniquePtr<UHRTDefaultHitGroupShader> RTDefaultHitGroupShader;
	UniquePtr<UHSoftRTShadowShader> SoftRTShadowShader;
	UniquePtr<UHRTShadowShader> RTShadowShader;

	UniquePtr<UHRTVertexTable> RTVertexTable;
	UniquePtr<UHRTIndicesTable> RTIndicesTable;
	UniquePtr<UHRTIndicesTypeTable> RTIndicesTypeTable;
	UniquePtr<UHRTMaterialDataTable> RTMaterialDataTable;
	UniquePtr<UHRenderBuffer<int32_t>> IndicesTypeBuffer;

	uint32_t RTInstanceCount;

	// -------------------------------------------- Culling related -------------------------------------------- //
	std::vector<UHMeshRendererComponent*> OpaquesToRender;
	std::vector<UHMeshRendererComponent*> TranslucentsToRender;
};