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

// UH Engine's material class, each instance is unique
class UHMaterial : public UHObject, public UHRenderState
{
public:
	UHMaterial();

	bool Import(std::filesystem::path InMatPath);
	void ImportGraphData(std::ifstream& FileIn);
	void PostImport();

#if WITH_DEBUG
	void SetTexFileName(UHMaterialTextureType TexType, std::string InName);
	void Export();
	void ExportGraphData(std::ofstream& FileOut);
	std::string GetTextureDefineCode(bool bIsDepthOrMotionPass);
	std::string GetMaterialInputCode(UHMaterialCompileData InData);
#endif

	void SetName(std::string InName);
	void SetCullMode(VkCullModeFlagBits InCullMode);
	void SetBlendMode(UHBlendMode InBlendMode);
	void SetMaterialProps(UHMaterialProperty InProp);
	void SetTex(UHMaterialTextureType InType, UHTexture* InTex);
	void SetSampler(UHMaterialTextureType InType, UHSampler* InSampler);
	void SetShader(UHMaterialShaderType InType, UHShader* InShader);
	void SetTextureIndex(UHMaterialTextureType InType, int32_t InIndex);
	void SetIsSkybox(bool InFlag);
	void SetCompileFlag(UHMaterialCompileFlag InFlag);

	std::string GetName() const;
	VkCullModeFlagBits GetCullMode() const;
	UHBlendMode GetBlendMode() const;
	UHMaterialProperty GetMaterialProps() const;
	UHMaterialCompileFlag GetCompileFlag() const;
	UHMaterialVersion GetVersion() const;
	std::filesystem::path GetPath() const;
	bool IsSkybox() const;

	std::string GetTexFileName(UHMaterialTextureType InType) const;
	UHTexture* GetTex(UHMaterialTextureType InType) const;
	UHSampler* GetSampler(UHMaterialTextureType InType) const;
	UHShader* GetShader(UHMaterialShaderType InType) const;
	int32_t GetTextureIndex(UHMaterialTextureType InType) const;
	std::string GetTexDefineName(UHMaterialTextureType InType) const;
	std::vector<std::string> GetMaterialDefines(UHMaterialShaderType InType) const;
	std::vector<std::string> GetRegisteredTextureNames(bool bIsDepthOrMotionPass);

	bool operator==(const UHMaterial& InMat);

#if WITH_DEBUG
	void GenerateDefaultMaterialNodes();
	std::unique_ptr<UHMaterialNode>& GetMaterialNode();
	std::vector<std::unique_ptr<UHGraphNode>>& GetEditNodes();

	void SetGUIRelativePos(std::vector<POINT> InPos);
	std::vector<POINT>& GetGUIRelativePos();
	void SetDefaultMaterialNodePos(POINT InPos);
	POINT GetDefaultMaterialNodePos();
#endif

private:
	UHMaterialVersion Version;
	std::string Name;
	std::array<std::string, UHMaterialTextureType::TextureTypeMax> TexFileNames;
	std::vector<std::string> RegisteredTextureNames;

	// material state variables
	VkCullModeFlagBits CullMode;
	UHBlendMode BlendMode;

	// material flags
	UHMaterialCompileFlag CompileFlag;
	bool bIsSkybox;
	bool bIsTangentSpace;

	UHMaterialProperty MaterialProps;
	std::array<UHTexture*, UHMaterialTextureType::TextureTypeMax> Textures;
	std::array<UHSampler*, UHMaterialTextureType::TextureTypeMax> Samplers;
	std::array<int32_t, UHMaterialTextureType::TextureTypeMax> TextureIndex;
	std::array<UHShader*, UHMaterialShaderType::MaterialShaderTypeMax> Shaders;
	std::array<std::string, UHMaterialTextureType::TextureTypeMax> TexDefines;

	std::unique_ptr<UHMaterialNode> MaterialNode;
	std::vector<std::unique_ptr<UHGraphNode>> EditNodes;
	std::vector<POINT> EditGUIRelativePos;

	// GUI positions relative to material node
	POINT DefaultMaterialNodePos;

	std::filesystem::path MaterialPath;
};