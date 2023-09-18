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
		FresnelFactor = 1.0f;
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

struct UHMaterialData
{
	int32_t TextureIndex;
	int32_t SamplerIndex;
	float Cutoff;
	float Padding;
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
	std::string GetTexFileName(UHMaterialInputs InType) const;
	void SetMaterialBufferSize(size_t InSize);
	void Export();
	void ExportGraphData(std::ofstream& FileOut);
	std::string GetCBufferDefineCode(size_t& OutSize);
	std::string GetMaterialInputCode(UHMaterialCompileData InData);
#endif

	void SetName(std::string InName);
	void SetMaterialProps(UHMaterialProperty InProp);
	void SetSystemTex(UHSystemTextureType InType, UHTexture* InTex);
	void SetSystemSampler(UHSystemTextureType InType, UHSampler* InSampler);
	void SetIsSkybox(bool InFlag);
	void SetCompileFlag(UHMaterialCompileFlag InFlag);
	void SetRegisteredTextureIndexes(std::vector<int32_t> InData);
	void AllocateMaterialBuffer();
	void UploadMaterialData(int32_t CurrFrame, const int32_t DefaultSamplerIndex);

	std::string GetName() const;
	UHCullMode GetCullMode() const;
	UHBlendMode GetBlendMode() const;
	UHMaterialProperty GetMaterialProps() const;
	UHMaterialCompileFlag GetCompileFlag() const;
	UHMaterialVersion GetVersion() const;
	std::filesystem::path GetPath() const;
	bool IsSkybox() const;
	bool IsOpaque() const;
	static bool IsDifferentBlendGroup(UHMaterial* InA, UHMaterial* InB);

	UHTexture* GetSystemTex(UHSystemTextureType InType) const;
	UHSampler* GetSystemSampler(UHSystemTextureType InType) const;
	std::vector<std::string> GetMaterialDefines();
	std::vector<std::string> GetRegisteredTextureNames();
	const std::array<UniquePtr<UHRenderBuffer<uint8_t>>, GMaxFrameInFlight>& GetMaterialConst();
	UHRenderBuffer<UHMaterialData>* GetRTMaterialDataGPU() const;

	bool operator==(const UHMaterial& InMat);

#if WITH_EDITOR
	void GenerateDefaultMaterialNodes();
	UniquePtr<UHMaterialNode>& GetMaterialNode();
	std::vector<UniquePtr<UHGraphNode>>& GetEditNodes();

	void SetGUIRelativePos(std::vector<POINT> InPos);
	std::vector<POINT>& GetGUIRelativePos();
	void SetDefaultMaterialNodePos(POINT InPos);
	POINT GetDefaultMaterialNodePos();
#endif

private:
	UHMaterialVersion Version;
	std::string Name;
	std::vector<std::string> RegisteredTextureNames;
	std::vector<int32_t> RegisteredTextureIndexes;

	// material state variables
	UHCullMode CullMode;
	UHBlendMode BlendMode;

	// material flags
	UHMaterialCompileFlag CompileFlag;
	bool bIsSkybox;
	bool bIsTangentSpace;

	UHMaterialProperty MaterialProps;
	std::array<UHTexture*, UHSystemTextureType::TextureTypeMax> SystemTextures;
	std::array<UHSampler*, UHSystemTextureType::TextureTypeMax> SystemSamplers;
	std::array<std::string, UHMaterialInputs::MaterialMax> TexFileNames;

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
	UniquePtr<UHRenderBuffer<UHMaterialData>> MaterialRTDataGPU;
};