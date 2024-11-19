#include "Shader.h"
#include "../../Runtime/Engine/Graphic.h"

UHShader::UHShader(std::string InShaderName, std::filesystem::path InSource, std::string InEntryName, std::string InProfileName
	, std::vector<std::string> InMacro)
	: Shader(nullptr)
	, ShaderName(InShaderName)
	, SourcePath(InSource)
	, EntryName(InEntryName)
	, ProfileName(InProfileName)
	, ShaderDefines(InMacro)
	, bIsMaterialShader(false)
{
	Name = ShaderName;
}

UHShader::UHShader(std::string InShaderName, std::filesystem::path InSource, std::string InEntryName, std::string InProfileName
	, bool bInIsMaterialShader, std::vector<std::string> InMacro)
	: Shader(nullptr)
	, ShaderName(InShaderName)
	, SourcePath(InSource)
	, EntryName(InEntryName)
	, ProfileName(InProfileName)
	, bIsMaterialShader(bInIsMaterialShader)
	, ShaderDefines(InMacro)
{
	Name = ShaderName;
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

bool UHShader::operator==(const UHShader& InShader)
{
	// check if shader defines are equal = size equal + individual equal
	bool bDefineEqual = ShaderDefines.size() == InShader.ShaderDefines.size();
	if (bDefineEqual)
	{
		for (size_t Idx = 0; Idx < ShaderDefines.size(); Idx++)
		{
			bDefineEqual &= ShaderDefines[Idx] == InShader.ShaderDefines[Idx];
		}
	}

	return bDefineEqual
		&& InShader.EntryName == EntryName
		&& InShader.ProfileName == ProfileName
		&& InShader.ShaderName == ShaderName
		&& InShader.SourcePath == SourcePath
		&& InShader.bIsMaterialShader == bIsMaterialShader;
}