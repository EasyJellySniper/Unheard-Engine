#pragma once
#include "Object.h"
#include "GraphicState.h"
#include "Types.h"
#include "Texture2D.h"
#include "Shader.h"
#include "Sampler.h"
#include "TextureCube.h"
#include "GraphNode/MaterialNode.h"

enum class UHMaterialVersion
{
	Initial,
	GoingBindless,
	AddRoughnessTexture,
	AddReflectionBounce,
	MaterialVersionMax
};

// UH material property, for import use
#if WITH_EDITOR
struct UHMaterialProperty
{
	UHMaterialProperty()
	{
		Diffuse = XMFLOAT3(0.8f, 0.8f, 0.8f);
		Opacity = 1.0f;
		Emissive = XMFLOAT3(0, 0, 0);
		EmissiveIntensity = 1.0f;

		Occlusion = 1.0f;
		Metallic = 0.0f;
		Roughness = 1.0f;
		Specular = XMFLOAT3(0.5f, 0.5f, 0.5f);
		BumpScale = 1.0f;
		FresnelFactor = 0.0f;
	}

	bool operator==(const UHMaterialProperty& InProp)
	{
		return MathHelpers::IsVectorEqual(InProp.Diffuse, Diffuse)
			&& InProp.Occlusion == Occlusion
			&& MathHelpers::IsVectorEqual(InProp.Emissive, Emissive)
			&& InProp.Metallic == Metallic
			&& InProp.Roughness == Roughness
			&& MathHelpers::IsVectorEqual(InProp.Specular, Specular)
			&& InProp.Opacity == Opacity
			&& InProp.EmissiveIntensity == EmissiveIntensity
			&& InProp.BumpScale == BumpScale
			&& InProp.FresnelFactor == FresnelFactor;
	}

	XMFLOAT3 Diffuse;
	float Opacity;
	XMFLOAT3 Emissive;
	float EmissiveIntensity;
	XMFLOAT3 Specular;
	float Occlusion;
	float Metallic;
	float Roughness;
	float BumpScale;
	float FresnelFactor;
};
#endif

// material data slot, the number in UHRTCommon.hlsli needs to match this
static const int32_t GMaxRTMaterialDataSlot = 32;
struct UHRTMaterialData
{
	uint32_t Data[GMaxRTMaterialDataSlot];
};

// UH Engine's material class, each instance is unique
class UHMaterial : public UHRenderResource, public UHRenderState
{
public:
	STATIC_CLASS_ID(35123443)
	UHMaterial();
	~UHMaterial();

	bool Import(std::filesystem::path InMatPath);
	void ImportGraphData(std::ifstream& FileIn);
	void PostImport();

	void SetName(std::string InName);
	void SetCompileFlag(UHMaterialCompileFlag InFlag);
	void SetRegisteredTextureIndexes(std::vector<int32_t> InData);
	void AllocateMaterialBuffer();
	void AllocateRTMaterialBuffer();
	void UploadMaterialData(int32_t CurrFrame);
	void UpdateMaterialUsage();

	std::string GetName() const;
	std::string GetSourcePath() const;
	UHCullMode GetCullMode() const;
	UHBlendMode GetBlendMode() const;
	UHMaterialCompileFlag GetCompileFlag() const;
	std::filesystem::path GetPath() const;
	bool IsOpaque() const;
	UHMaterialUsage GetMaterialUsages() const;
	std::vector<std::string> GetShaderDefines();

	static bool IsDifferentBlendGroup(UHMaterial* InA, UHMaterial* InB);

	const std::vector<std::string>& GetRegisteredTextureNames();
	const std::array<UniquePtr<UHRenderBuffer<uint8_t>>, GMaxFrameInFlight>& GetMaterialConst();
	UHRenderBuffer<UHRTMaterialData>* GetRTMaterialDataGPU(int32_t CurrFrame) const;

	bool operator==(const UHMaterial& InMat);

#if WITH_EDITOR
	// setting cull mode & blend mode is only available in editor
	void SetCullMode(UHCullMode InCullMode);
	void SetBlendMode(UHBlendMode InBlendMode);

	void SetTexFileName(UHMaterialInputs TexType, std::string InName);
	void SetMaterialBufferSize(size_t InSize);
	void Export(const std::filesystem::path InPath = "");
	void ExportGraphData(std::ofstream& FileOut);
	std::string GetCBufferDefineCode(size_t& OutSize);
	std::string GetMaterialInputCode(UHMaterialCompileData InData);

	void SetMaterialProps(UHMaterialProperty InProp);
	UHMaterialProperty GetMaterialProps() const;

	void GenerateDefaultMaterialNodes();
	UniquePtr<UHMaterialNode>& GetMaterialNode();
	std::vector<UniquePtr<UHGraphNode>>& GetEditNodes();

	void SetGUIRelativePos(std::vector<POINT> InPos);
	std::vector<POINT>& GetGUIRelativePos();
	void SetDefaultMaterialNodePos(POINT InPos);
	POINT GetDefaultMaterialNodePos();
	void SetSourcePath(const std::string InPath);

	bool IsMaterialNodeDirty() const;
#endif

private:

	std::vector<std::string> RegisteredTextureNames;
	std::vector<int32_t> RegisteredTextureIndexes;
	std::string SourcePath;

	// material state variables
	UHCullMode CullMode;
	UHBlendMode BlendMode;
	float CutoffValue;
	uint32_t MaxReflectionBounce;

	// material flags
	UHMaterialCompileFlag CompileFlag;
	UHMaterialUsage MaterialUsages;
	std::string TexFileNames[UH_ENUM_VALUE(UHMaterialInputs::MaterialMax)];

	UniquePtr<UHMaterialNode> MaterialNode;
	std::vector<UniquePtr<UHGraphNode>> EditNodes;
	std::vector<POINT> EditGUIRelativePos;

	// GUI positions relative to material node
	POINT DefaultMaterialNodePos;
	std::filesystem::path MaterialPath;

	// material constant buffer, the size will be following the result of graph
	size_t MaterialBufferSize;
	std::vector<uint8_t> MaterialConstantsCPU;
	std::array<UniquePtr<UHRenderBuffer<uint8_t>>, GMaxFrameInFlight> MaterialConstantsGPU;

	UHRTMaterialData MaterialRTDataCPU;
	UniquePtr<UHRenderBuffer<UHRTMaterialData>> MaterialRTDataGPU[GMaxFrameInFlight];

#if WITH_EDITOR
	UHMaterialProperty MaterialProps;
	bool bIsMaterialNodeDirty;
	friend class UHMaterialDialog;
#endif
};