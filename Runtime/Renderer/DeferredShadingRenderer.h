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
#include <memory>
#include <unordered_map>

// shader includes
#include "ShaderClass/DepthPassShader.h"
#include "ShaderClass/BasePassShader.h"
#include "ShaderClass/LightPassShader.h"
#include "ShaderClass/SkyPassShader.h"
#include "ShaderClass/MotionPassShader.h"
#include "ShaderClass/PostProcessing/ToneMappingShader.h"
#include "ShaderClass/PostProcessing/TemporalAAShader.h"
#include "ShaderClass/RayTracing/RTDefaultHitGroupShader.h"
#include "ShaderClass/RayTracing/RTOcclusionTestShader.h"
#include "ShaderClass/RayTracing/RTShadowShader.h"
#include "ShaderClass/RayTracing/RTTextureTable.h"
#include "ShaderClass/RayTracing/RTMeshTable.h"

#if WITH_DEBUG
#include "ShaderClass/PostProcessing/DebugViewShader.h"
#include "../../Editor/Editor/Profiler.h"
#endif

enum UHRenderTask
{
	None = -1,
	DepthPassTask,
	BasePassTask
};

// Deferred Shading Renderer class for Unheard Engine, initialize with a UHGraphic pointer and a asset pointer
class UHDeferredShadingRenderer
{
public:
	UHDeferredShadingRenderer(UHGraphic* InGraphic, UHAssetManager* InAssetManager, UHConfigManager* InConfig, UHGameTimer* InTimer);
	bool Initialize(UHScene* InScene);
	void Release();

	void SetCurrentScene(UHScene* InScene);
	bool IsNeedReset();

	void Resize();
	void Reset();
	void Update();
	void NotifyRenderThread();

	// only resize RT buffers
	void ResizeRTBuffers();

#if WITH_DEBUG
	void SetDebugViewIndex(int32_t Idx);
	float GetRenderThreadTime() const;
	int32_t GetDrawCallCount() const;
	std::array<float, UHRenderPassTypes::UHRenderPassMax> GetGPUTimes() const;
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

	// prepare rendering shaders
	void PrepareRenderingShaders();

	// create command pool and buffer
	bool CreateMainCommandPoolAndBuffer();

	// update descriptors
	void UpdateDescriptors();

	// release shaders
	void ReleaseShaders();

	// create fences
	bool CreateFences();

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

#if WITH_DEBUG
	void RefreshMaterialShaders();
#endif


	/************************************************ rendering functions ************************************************/
	void BuildTopLevelAS(UHGraphicBuilder& GraphBuilder);
	void DispatchRayOcclusionTestPass(UHGraphicBuilder& GraphBuilder);
	void RenderDepthPrePass(UHGraphicBuilder& GraphBuilder);
	void RenderBasePass(UHGraphicBuilder& GraphBuilder);
	void DispatchRayShadowPass(UHGraphicBuilder& GraphBuilder);
	void RenderLightPass(UHGraphicBuilder& GraphBuilder);
	void RenderSkyPass(UHGraphicBuilder& GraphBuilder);
	void RenderMotionPass(UHGraphicBuilder& GraphBuilder);
	void RenderEffect(UHShaderClass* InShader, UHGraphicBuilder& GraphBuilder, int32_t& PostProcessIdx, std::string InName, int32_t ImgBinding);
	void RenderPostProcessing(UHGraphicBuilder& GraphBuilder);
	uint32_t RenderSceneToSwapChain(UHGraphicBuilder& GraphBuilder);


	/************************************************ rendering task functions ************************************************/
	void DepthPassTask(int32_t ThreadIdx);
	void BasePassTask(int32_t ThreadIdx);


	/************************************************ variables ************************************************/

	UHGraphic* GraphicInterface;
	UHAssetManager* AssetManagerInterface;
	UHConfigManager* ConfigInterface;
	UHGameTimer* TimerInterface;
	VkExtent2D RenderResolution;
	VkExtent2D RTShadowExtent;

	// similar to D3D12 command list allocator
	VkCommandPool MainCommandPool;

	// similar to D3D12 command list
	std::array<VkCommandBuffer, GMaxFrameInFlight> MainCommandBuffers;

	UHParallelSubmitter DepthParallelSubmitter;
	UHParallelSubmitter BaseParallelSubmitter;

	// similar to D3D12 Fence (GPU Fence)
	std::array<VkSemaphore, GMaxFrameInFlight> SwapChainAvailableSemaphores;
	std::array<VkSemaphore, GMaxFrameInFlight> RenderFinishedSemaphores;

	// similar to D3D12 Fence
	std::array<VkFence, GMaxFrameInFlight> MainFences;

	// current frame count
	uint32_t CurrentFrame = 0;

	// Render thread defines, UH engine will always use a thread for rendering, and doing parallel submission with worker threads
	std::unique_ptr<UHThread> RenderThread;
	int32_t NumWorkerThreads;
	std::vector<std::unique_ptr<UHThread>> WorkerThreads;
	bool bIsResetNeededShared;
	UHRenderTask RenderTask;
	bool bParallelSubmissionGT;
	bool bParallelSubmissionRT;

	// current scene
	UHScene* CurrentScene;

	// system constant
	UHSystemConstants SystemConstantsCPU;
	std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight> SystemConstantsGPU;

	// object & material constants, I'll create constant buffer which are big enough for all renderers
	std::array<std::unique_ptr<UHRenderBuffer<UHObjectConstants>>, GMaxFrameInFlight> ObjectConstantsGPU;
	std::array<std::unique_ptr<UHRenderBuffer<UHMaterialConstants>>, GMaxFrameInFlight> MaterialConstantsGPU;

	// light buffers, this will be used as structure buffer instead of constant
	std::array<std::unique_ptr<UHRenderBuffer<UHDirectionalLightConstants>>, GMaxFrameInFlight> DirectionalLightBuffer;

	// shared samplers
	UHSampler* PointClampedSampler;
	UHSampler* LinearClampedSampler;
	UHSampler* AnisoClampedSampler;


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

	UHRenderTexture* SceneDiffuse;
	UHRenderTexture* SceneNormal;
	UHRenderTexture* SceneMaterial;
	UHRenderTexture* SceneResult;
	UHRenderTexture* SceneMip;
	UHRenderTexture* SceneDepth;

	// store different base pass object, the id is renderer data index
	std::unordered_map<int32_t, UHBasePassShader> BasePassShaders;
	UHRenderPassObject BasePassObj;

	// -------------------------------------------- Light Pass -------------------------------------------- //
	UHLightPassShader LightPassShader;
	UHRenderPassObject LightPassObj;

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
	UHRenderPassObject MotionObjectPassObj;
	std::unordered_map<int32_t, UHMotionObjectPassShader> MotionObjectShaders;

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

	// profiles
	float RenderThreadTime;
	int32_t DrawCalls;
	std::vector<int32_t> ThreadDrawCalls;
	std::array<UHGPUQuery*, UHRenderPassTypes::UHRenderPassMax> GPUTimeQueries;
	std::array<float, UHRenderPassTypes::UHRenderPassMax> GPUTimes;
#endif

	// -------------------------------------------- Ray tracing related -------------------------------------------- //
	VkFormat RTShadowFormat;
	std::array<std::unique_ptr<UHAccelerationStructure>, GMaxFrameInFlight> TopLevelAS;
	UHRTDefaultHitGroupShader RTDefaultHitGroupShader;

	UHRTOcclusionTestShader RTOcclusionTestShader;
	std::array<std::unique_ptr<UHRenderBuffer<uint32_t>>, GMaxFrameInFlight> OcclusionVisibleBuffer;

	UHRTShadowShader RTShadowShader;
	UHRenderTexture* RTShadowResult;

	UHRTTextureTable RTTextureTable;
	UHRTSamplerTable RTSamplerTable;
	UHRTVertexTable RTVertexTable;
	UHRTIndicesTable RTIndicesTable;
	UHRTIndicesTypeTable RTIndicesTypeTable;
	std::unique_ptr<UHRenderBuffer<int32_t>> IndicesTypeBuffer;

	uint32_t RTInstanceCount;

	// -------------------------------------------- Culling related -------------------------------------------- //
	std::vector<UHMeshRendererComponent*> OpaquesToRender;
	std::vector<UHMeshRendererComponent*> TranslucentsToRender;
};