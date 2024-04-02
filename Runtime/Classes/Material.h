#pragma once
#include "Object.h"
#include "GraphicState.h"
#include "Types.h"
#include "Texture2D.h"
#include "Shader.h"
#include "Sampler.h"
#include "TextureCube.h"
#include "GraphNode/MaterialNode.h"

enum UHMaterialVersion
{
	Initial,
	GoingBindless,
	AddRoughnessTexture,
	MaterialVersionMax
};

// UH material property, for CPU use
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
		Cutoff = 0.33f;
		FresnelFactor = 0.0f;
		ReflectionFactor = 1.0f;
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
			&& InProp.Cutoff == Cutoff
			&& InProp.FresnelFactor == FresnelFactor
			&& InProp.ReflectionFactor == ReflectionFactor;
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
	float Cutoff;
	float FresnelFactor;
	float ReflectionFactor;
};

// material data, max available number of scalars are 128 for now, the shader needs to match this number
static const int32_t GMaxRTMaterialDataSlot = 128;
struct UHRTMaterialData
{
	uint32_t Data[GMaxRTMaterialDataSlot];
};

// UH Engine's material class, each instance is unique
class UHMaterial : public UHRenderResource, public UHRenderState
{
public:
	UHMaterial();
	~UHMaterial();

	bool Import(std::filesystem::path InMatPath);
	void ImportGraphData(std::ifstream& FileIn);
	void PostImport();

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
#endif

	void SetName(std::string InName);
	void SetMaterialProps(UHMaterialProperty InProp);
	void SetIsSkybox(bool InFlag);
	void SetCompileFlag(UHMaterialCompileFlag InFlag);
	void SetRegisteredTextureIndexes(std::vector<int32_t> InData);
	void AllocateMaterialBuffer();
	void AllocateRTMaterialBuffer();
	void UploadMaterialData(int32_t CurrFrame);

	std::string GetName() const;
	std::string GetSourcePath() const;
	UHCullMode GetCullMode() const;
	UHBlendMode GetBlendMode() const;
	UHMaterialProperty GetMaterialProps() const;
	UHMaterialCompileFlag GetCompileFlag() const;
	std::filesystem::path GetPath() const;
	bool IsOpaque() const;
	UHMaterialUsage GetMaterialUsages() const;

	static bool IsDifferentBlendGroup(UHMaterial* InA, UHMaterial* InB);

	std::vector<std::string> GetRegisteredTextureNames();
	const std::array<UniquePtr<UHRenderBuffer<uint8_t>>, GMaxFrameInFlight>& GetMaterialConst();
	UHRenderBuffer<UHRTMaterialData>* GetRTMaterialDataGPU(int32_t CurrFrame) const;

	bool operator==(const UHMaterial& InMat);

#if WITH_EDITOR
	void GenerateDefaultMaterialNodes();
	UniquePtr<UHMaterialNode>& GetMaterialNode();
	std::vector<UniquePtr<UHGraphNode>>& GetEditNodes();

	void SetGUIRelativePos(std::vector<POINT> InPos);
	std::vector<POINT>& GetGUIRelativePos();
	void SetDefaultMaterialNodePos(POINT InPos);
	POINT GetDefaultMaterialNodePos();
	void SetSourcePath(const std::string InPath);
#endif

private:
	std::vector<std::string> RegisteredTextureNames;
	std::vector<int32_t> RegisteredTextureIndexes;
	std::string SourcePath;

	// material state variables
	UHCullMode CullMode;
	UHBlendMode BlendMode;

	// material flags
	UHMaterialCompileFlag CompileFlag;
	UHMaterialUsage MaterialUsages;

	UHMaterialProperty MaterialProps;
	std::string TexFileNames[UHMaterialInputs::MaterialMax];

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
};