#include "Shader.h"
#include "../../Runtime/Engine/Graphic.h"
#include "AssetPath.h"

UHShader::UHShader(std::string InShaderName, std::filesystem::path InSource, std::string InEntryName, std::string InProfileName
	, std::vector<std::string> InMacro)
	: UHShader(InShaderName, InSource, InEntryName, InProfileName, "", InMacro)
{
	
}

UHShader::UHShader(std::string InShaderName, std::filesystem::path InSource, std::string InEntryName, std::string InProfileName
	, std::string InMaterialName, std::vector<std::string> InMacro)
	: Shader(nullptr)
	, ShaderName(InShaderName)
	, SourcePath(InSource)
	, EntryName(InEntryName)
	, ProfileName(InProfileName)
	, bIsMaterialShader(InMaterialName != "")
	, ShaderDefines(InMacro)
{
	Name = ShaderName;

	// Generate shader hash by string, format in shader name + entry name + macro name
	// Or an extra material name if it's a material shader
	std::string Key = InShaderName + InEntryName;
	for (const std::string& Str : InMacro)
	{
		Key += Str;
	}

	if (bIsMaterialShader)
	{
		Key += InMaterialName;
	}

	ShaderHash = UHUtilities::StringToHash(Key);
}

void UHShader::Release()
{
	vkDestroyShaderModule(LogicalDevice, Shader, nullptr);
}

bool UHShader::Create(VkShaderModuleCreateInfo InCreateInfo)
{
	if (vkCreateShaderModule(LogicalDevice, &InCreateInfo, nullptr, &Shader) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to create shader module!\n");
		return false;
	}

#if WITH_EDITOR
	GfxCache->SetDebugUtilsObjectName(VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)Shader, Name);
#endif

	return true;
}

VkShaderModule UHShader::GetShader() const
{
	return Shader;
}

std::string UHShader::GetEntryName() const
{
	return EntryName;
}

std::string UHShader::GetProfileName() const
{
	return ProfileName;
}

std::vector<std::string> UHShader::GetShaderDefines() const
{
	return ShaderDefines;
}

size_t UHShader::GetShaderHash() const
{
	return ShaderHash;
}

std::filesystem::path UHShader::GetSourcePath() const
{
	return SourcePath;
}

std::filesystem::path UHShader::GetOutputPath() const
{
	const std::string OriginSubpath = UHAssetPath::GetShaderOriginSubpath(SourcePath);
	const std::string ShaderHashName = std::to_string(ShaderHash);
	std::filesystem::path OutputPath = GShaderAssetFolder + OriginSubpath + ShaderHashName + GShaderAssetExtension;

	return OutputPath;
}

bool UHShader::operator==(const UHShader& InShader)
{
	return InShader.ShaderHash == ShaderHash 
		&& InShader.ProfileName == ProfileName
		&& InShader.SourcePath == SourcePath
		&& InShader.bIsMaterialShader == bIsMaterialShader;
}