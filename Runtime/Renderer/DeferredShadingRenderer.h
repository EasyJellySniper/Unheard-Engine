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
#include "GraphicBuilder.h"
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

#if WITH_DEBUG
#include "ShaderClass/PostProcessing/DebugViewShader.h"
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
	void SetSwapChainReset(bool bInFlag);
	bool IsNeedReset();

	void Resize();
	void Reset();
	void Update();
	void NotifyRenderThread();

	// only resize RT buffers
	void ResizeRayTracingBuffers();
	void UpdateTextureDescriptors();

#if WITH_DEBUG
	void SetDebugViewIndex(int32_t Idx);
	float GetRenderThreadTime() const;
	int32_t GetDrawCallCount() const;
	std::array<float, UHRenderPassTypes::UHRenderPassMax> GetGPUTimes() const;
	void RefreshMaterialShaders(UHMaterial* InMat, bool bNeedReassignRendererGroup = false);
#endif

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


	/************************************************ rendering functions ************************************************/
	void BuildTopLevelAS(UHGraphicBuilder& GraphBuilder);
	void RenderDepthPrePass(UHGraphicBuilder& GraphBuilder);
	void RenderBasePass(UHGraphicBuilder& GraphBuilder);
	void DispatchLightCulling(UHGraphicBuilder& GraphBuilder);
	void DispatchRayShadowPass(UHGraphicBuilder& GraphBuilder);
	void RenderLightPass(UHGraphicBuilder& GraphBuilder);
	void RenderSkyPass(UHGraphicBuilder& GraphBuilder);
	void RenderMotionPass(UHGraphicBuilder& GraphBuilder);
	void RenderTranslucentPass(UHGraphicBuilder& GraphBuilder);
	void RenderEffect(UHShaderClass* InShader, UHGraphicBuilder& GraphBuilder, int32_t& PostProcessIdx, std::string InName);
	void DispatchEffect(UHShaderClass* InShader, UHGraphicBuilder& GraphBuilder, int32_t& PostProcessIdx, std::string InName);
	void RenderPostProcessing(UHGraphicBuilder& GraphBuilder);
	uint32_t RenderSceneToSwapChain(UHGraphicBuilder& GraphBuilder);


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

	// current scene
	UHScene* CurrentScene;

	// system constant
	UHSystemConstants SystemConstantsCPU;
	std::array<UniquePtr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight> SystemConstantsGPU;

	// object & material constants, I'll create constant buffer which are big enough for all renderers
	std::vector<UHObjectConstants> ObjectConstantsCPU;
	std::array<UniquePtr<UHRenderBuffer<UHObjectConstants>>, GMaxFrameInFlight> ObjectConstantsGPU;

	// light buffers, this will be used as structure buffer instead of constant
	std::vector<UHDirectionalLightConstants> DirLightConstantsCPU;
	std::array<UniquePtr<UHRenderBuffer<UHDirectionalLightConstants>>, GMaxFrameInFlight> DirectionalLightBuffer;
	std::vector<UHPointLightConstants> PointLightConstantsCPU;
	std::array<UniquePtr<UHRenderBuffer<UHPointLightConstants>>, GMaxFrameInFlight> PointLightBuffer;

	// light culling buffers
	UniquePtr<UHRenderBuffer<uint32_t>> PointLightListBuffer;
	UniquePtr<UHRenderBuffer<uint32_t>> PointLightListTransBuffer;

	// shared samplers
	UHSampler* PointClampedSampler;
	UHSampler* LinearClampedSampler;
	UHSampler* AnisoClampedSampler;
	int32_t DefaultSamplerIndex;

	// bindless table
	UHTextureTable TextureTable;
	UHSamplerTable SamplerTable;


	/************************************************ Render Pass stuffs ************************************************/

	// -------------------------------------------- Depth Pass -------------------------------------------- //
	std::unordered_map<int32_t, UHDepthPassShader> DepthPassShaders;
	UHRenderPassObject DepthPassObj;
	bool bEnableDepthPrePass;

	// -------------------------------------------- Base Pass -------------------------------------------- //

	// GBuffers
	VkFormat DiffuseFormat;
	VkFormat NormalFormat;
	VkFormat SpecularFormat;
	VkFormat SceneResultFormat;
	VkFormat SceneMipFormat;
	VkFormat DepthFormat;
	VkFormat HDRFormat;

	UHRenderTexture* SceneDiffuse;
	UHRenderTexture* SceneNormal;
	UHRenderTexture* SceneMaterial;
	UHRenderTexture* SceneResult;
	UHRenderTexture* SceneMip;
	UHRenderTexture* SceneDepth;
	UHRenderTexture* SceneTranslucentDepth;
	UHRenderTexture* SceneVertexNormal;
	UHRenderTexture* SceneTranslucentVertexNormal;

	// store different base pass object, the id is renderer data index
	std::unordered_map<int32_t, UHBasePassShader> BasePassShaders;
	UHRenderPassObject BasePassObj;

	// -------------------------------------------- Light and Light Culling Pass -------------------------------------------- //
	const uint32_t LightCullingTileSize;
	const uint32_t MaxPointLightPerTile;
	UHLightCullingShader LightCullingShader;
	UHLightPassShader LightPassShader;

	// -------------------------------------------- Skybox Pass -------------------------------------------- //
	UHSkyPassShader SkyPassShader;
	UHRenderPassObject SkyboxPassObj;

	// -------------------------------------------- Motion vector Pass -------------------------------------------- //
	VkFormat MotionFormat;
	UHRenderTexture* MotionVectorRT;
	UHRenderTexture* PrevMotionVectorRT;
	UHRenderPassObject MotionCameraPassObj;
	UHMotionCameraPassShader MotionCameraShader;

	// store different motion pass object, thd id is buffer data index(per renderer)
	// the motion shader is separate into opaque and translucent
	UHRenderPassObject MotionOpaquePassObj;
	std::unordered_map<int32_t, UHMotionObjectPassShader> MotionOpaqueShaders;
	UHRenderPassObject MotionTranslucentPassObj;
	std::unordered_map<int32_t, UHMotionObjectPassShader> MotionTranslucentShaders;

	// -------------------------------------------- Translucent Pass -------------------------------------------- //
	std::unordered_map<int32_t, UHTranslucentPassShader> TranslucentPassShaders;
	UHRenderPassObject TranslucentPassObj;

	// -------------------------------------------- Post processing Pass -------------------------------------------- //
	// post process needs to use two textures and keep blit to each other, so the effects can be accumulated
	static const int32_t NumOfPostProcessRT = 2;
	int32_t PostProcessResultIdx;
	std::array<UHRenderPassObject, NumOfPostProcessRT> PostProcessPassObj;
	UHRenderTexture* PostProcessRT;
	std::array<UHRenderTexture*, NumOfPostProcessRT> PostProcessResults;
	UHRenderTexture* PreviousSceneResult;

	UHToneMappingShader ToneMapShader;
	UHTemporalAAShader TemporalAAShader;
	bool bIsTemporalReset;

#if WITH_DEBUG
	// debug view shader
	UHDebugViewShader DebugViewShader;
	int32_t DebugViewIndex;
	UniquePtr<UHRenderBuffer<uint32_t>> DebugViewData;

	// profiles
	float RenderThreadTime;
	int32_t DrawCalls;
	std::vector<int32_t> ThreadDrawCalls;
	std::array<float, UHRenderPassTypes::UHRenderPassMax> GPUTimes;
#endif
	std::array<UHGPUQuery*, UHRenderPassTypes::UHRenderPassMax> GPUTimeQueries;

	// -------------------------------------------- Ray tracing related -------------------------------------------- //
	VkFormat RTShadowFormat;
	std::array<UniquePtr<UHAccelerationStructure>, GMaxFrameInFlight> TopLevelAS;

	UHRTDefaultHitGroupShader RTDefaultHitGroupShader;
	UHSoftRTShadowShader SoftRTShadowShader;
	UHRTShadowShader RTShadowShader;

	UHRenderTexture* RTShadowResult;

	// shared texture aim for being reused during rendering
	UHRenderTexture* RTSharedTextureRG16F;

	UHRTVertexTable RTVertexTable;
	UHRTIndicesTable RTIndicesTable;
	UHRTIndicesTypeTable RTIndicesTypeTable;
	UHRTMaterialDataTable RTMaterialDataTable;
	UniquePtr<UHRenderBuffer<int32_t>> IndicesTypeBuffer;

	uint32_t RTInstanceCount;

	// -------------------------------------------- Culling related -------------------------------------------- //
	std::vector<UHMeshRendererComponent*> OpaquesToRender;
	std::vector<UHMeshRendererComponent*> TranslucentsToRender;
};