#include "Material.h"
#include <fstream>
#include <filesystem>
#include "Utility.h"
#include "AssetPath.h"

// default as opaque material and set cull off for now
UHMaterial::UHMaterial()
	: CullMode(VK_CULL_MODE_NONE)
	, BlendMode(UHBlendMode::Opaque)
	, ShadingModel(UHShadingModel::DefaultLit)
	, MaterialProps(UHMaterialProperty())
	, bIsSkybox(false)
{
	for (int32_t Idx = 0; Idx < UHMaterialTextureType::TextureTypeMax; Idx++)
	{
		Textures[Idx] = nullptr;
		Samplers[Idx] = nullptr;
		TextureIndex[Idx] = -1;
	}

	for (int32_t Idx = 0; Idx < UHMaterialShaderType::MaterialShaderTypeMax; Idx++)
	{
		Shaders[Idx] = nullptr;
	}

	TexDefines = { "WITH_DIFFUSE", "WITH_OCCLUSION","WITH_SPECULAR","WITH_NORMAL","WITH_OPACITY", "WITH_ENVCUBE", "WITH_METALLIC", "WITH_ROUGHNESS" };
}

bool UHMaterial::Import(std::filesystem::path InMatPath)
{
	std::ifstream FileIn(InMatPath, std::ios::in | std::ios::binary);

	if (!FileIn.is_open())
	{
		// skip if failed to open
		return false;
	}

	// load material data
	UHUtilities::ReadStringData(FileIn, Name);

	// load referenced texture file name, doesn't read file name for sky cube at the moment
	for (int32_t Idx = 0; Idx <= UHMaterialTextureType::Opacity; Idx++)
	{
		UHUtilities::ReadStringData(FileIn, TexFileNames[Idx]);
	}

	FileIn.read(reinterpret_cast<char*>(&CullMode), sizeof(CullMode));
	FileIn.read(reinterpret_cast<char*>(&BlendMode), sizeof(BlendMode));
	FileIn.read(reinterpret_cast<char*>(&MaterialProps), sizeof(MaterialProps));

	FileIn.close();

	return true;
}

#if WITH_DEBUG
void UHMaterial::SetTexFileName(UHMaterialTextureType TexType, std::string InName)
{
	TexFileNames[TexType] = InName;
}

void UHMaterial::Export()
{
	// export UH Material
	// 
	// create folder if it's not existed
	if (!std::filesystem::exists(GMaterialAssetPath))
	{
		std::filesystem::create_directories(GMaterialAssetPath);
	}

	std::ofstream FileOut(GMaterialAssetPath + Name + GMaterialAssetExtension, std::ios::out | std::ios::binary);

	// write material name
	UHUtilities::WriteStringData(FileOut, Name);

	// write texture filename used, doesn't write file name for sky cube/metallic at the moment
	for (int32_t Idx = 0; Idx <= UHMaterialTextureType::Opacity; Idx++)
	{
		UHUtilities::WriteStringData(FileOut, TexFileNames[Idx]);
	}

	// write cullmode and blend mode
	FileOut.write(reinterpret_cast<const char*>(&CullMode), sizeof(CullMode));
	FileOut.write(reinterpret_cast<const char*>(&BlendMode), sizeof(BlendMode));

	// write constants
	FileOut.write(reinterpret_cast<const char*>(&MaterialProps), sizeof(MaterialProps));

	FileOut.close();
}
#endif

void UHMaterial::SetName(std::string InName)
{
	Name = InName;
}

void UHMaterial::SetCullMode(VkCullModeFlagBits InCullMode)
{
	// use for initialization at the moment
	// need to support runtime change in the future
	CullMode = InCullMode;
}

void UHMaterial::SetBlendMode(UHBlendMode InBlendMode)
{
	// use for initialization at the moment
	// need to support runtime change in the future
	BlendMode = InBlendMode;
}

void UHMaterial::SetShadingModel(UHShadingModel InShadingModel)
{
	ShadingModel = InShadingModel;
}

void UHMaterial::SetMaterialProps(UHMaterialProperty InProp)
{
	MaterialProps = InProp;
	SetRenderDirties(true);
}

void UHMaterial::SetTex(UHMaterialTextureType InType, UHTexture* InTex)
{
	Textures[InType] = InTex;
}

void UHMaterial::SetSampler(UHMaterialTextureType InType, UHSampler* InSampler)
{
	Samplers[InType] = InSampler;
}

void UHMaterial::SetShader(UHMaterialShaderType InType, UHShader* InShader)
{
	Shaders[InType] = InShader;
}

void UHMaterial::SetTextureIndex(UHMaterialTextureType InType, int32_t InIndex)
{
	TextureIndex[InType] = InIndex;
}

void UHMaterial::SetIsSkybox(bool InFlag)
{
	bIsSkybox = InFlag;
}

std::string UHMaterial::GetName() const
{
	return Name;
}

VkCullModeFlagBits UHMaterial::GetCullMode() const
{
	return CullMode;
}

UHBlendMode UHMaterial::GetBlendMode() const
{
	return BlendMode;
}

UHMaterialProperty UHMaterial::GetMaterialProps() const
{
	return MaterialProps;
}

UHShadingModel UHMaterial::GetShadingModel() const
{
	return ShadingModel;
}

bool UHMaterial::IsSkybox() const
{
	return bIsSkybox;
}

std::string UHMaterial::GetTexFileName(UHMaterialTextureType InType) const
{
	return TexFileNames[InType];
}

UHTexture* UHMaterial::GetTex(UHMaterialTextureType InType) const
{
	return Textures[InType];
}

UHSampler* UHMaterial::GetSampler(UHMaterialTextureType InType) const
{
	return Samplers[InType];
}

UHShader* UHMaterial::GetShader(UHMaterialShaderType InType) const
{
	return Shaders[InType];
}

int32_t UHMaterial::GetTextureIndex(UHMaterialTextureType InType) const
{
	return TextureIndex[InType];
}

std::string UHMaterial::GetTexDefineName(UHMaterialTextureType InType) const
{
	return TexDefines[InType];
}

std::vector<std::string> UHMaterial::GetMaterialDefines(UHMaterialShaderType InType) const
{
	std::vector<std::string> Defines;
	for (int32_t Idx = 0; Idx < UHMaterialTextureType::TextureTypeMax; Idx++)
	{
		if (Textures[Idx])
		{
			switch (InType)
			{
			case UHMaterialShaderType::VS:
				if (Idx == UHMaterialTextureType::Normal || Idx == UHMaterialTextureType::SkyCube)
				{
					Defines.push_back(TexDefines[Idx]);
				}
				break;
			case UHMaterialShaderType::PS:
				Defines.push_back(TexDefines[Idx]);
				break;
			}
		}
	}

	return Defines;
}

bool UHMaterial::operator==(const UHMaterial& InMat)
{
	return InMat.GetName() == Name
		&& InMat.GetCullMode() == CullMode
		&& InMat.GetBlendMode() == BlendMode
		&& InMat.GetMaterialProps() == MaterialProps;
}