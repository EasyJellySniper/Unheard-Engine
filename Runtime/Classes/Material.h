#pragma once
#include "Object.h"
#include "GraphicState.h"
#include "Types.h"
#include "Texture2D.h"
#include "Shader.h"
#include "Sampler.h"
#include "TextureCube.h"

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
#if WITH_DEBUG
	void SetTexFileName(UHMaterialTextureType TexType, std::string InName);
	void Export();
#endif

	void SetName(std::string InName);
	void SetCullMode(VkCullModeFlagBits InCullMode);
	void SetBlendMode(UHBlendMode InBlendMode);
	void SetShadingModel(UHShadingModel InShadingModel);
	void SetMaterialProps(UHMaterialProperty InProp);
	void SetTex(UHMaterialTextureType InType, UHTexture* InTex);
	void SetSampler(UHMaterialTextureType InType, UHSampler* InSampler);
	void SetShader(UHMaterialShaderType InType, UHShader* InShader);
	void SetTextureIndex(UHMaterialTextureType InType, int32_t InIndex);
	void SetIsSkybox(bool InFlag);

	std::string GetName() const;
	VkCullModeFlagBits GetCullMode() const;
	UHBlendMode GetBlendMode() const;
	UHMaterialProperty GetMaterialProps() const;
	UHShadingModel GetShadingModel() const;
	bool IsSkybox() const;

	std::string GetTexFileName(UHMaterialTextureType InType) const;
	UHTexture* GetTex(UHMaterialTextureType InType) const;
	UHSampler* GetSampler(UHMaterialTextureType InType) const;
	UHShader* GetShader(UHMaterialShaderType InType) const;
	int32_t GetTextureIndex(UHMaterialTextureType InType) const;
	std::string GetTexDefineName(UHMaterialTextureType InType) const;
	std::vector<std::string> GetMaterialDefines(UHMaterialShaderType InType) const;

	bool operator==(const UHMaterial& InMat);

private:
	std::string Name;
	std::array<std::string, UHMaterialTextureType::TextureTypeMax> TexFileNames;

	// material state variables
	VkCullModeFlagBits CullMode;
	UHBlendMode BlendMode;
	UHShadingModel ShadingModel;

	// skybox variable, the tex cube will be used as reflection source if it's not a skybox material
	bool bIsSkybox;

	UHMaterialProperty MaterialProps;
	std::array<UHTexture*, UHMaterialTextureType::TextureTypeMax> Textures;
	std::array<UHSampler*, UHMaterialTextureType::TextureTypeMax> Samplers;
	std::array<int32_t, UHMaterialTextureType::TextureTypeMax> TextureIndex;
	std::array<UHShader*, UHMaterialShaderType::MaterialShaderTypeMax> Shaders;
	std::array<std::string, UHMaterialTextureType::TextureTypeMax> TexDefines;
};