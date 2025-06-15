#pragma once
#include "../Engine/RenderResource.h"
#include "../Classes/Object.h"
#include "../../UnheardEngine.h"
#include <string>
#include <filesystem>

class UHGraphic;

// Shader class, need to request shader from UHGraphic
class UHShader : public UHRenderResource
{
public:
	UHShader(std::string InShaderName, std::filesystem::path InSource, std::string InEntryName, std::string InProfileName
		, std::vector<std::string> InMacro);
	UHShader(std::string InShaderName, std::filesystem::path InSource, std::string InEntryName, std::string InProfileName
		, std::string InMaterialName, std::vector<std::string> InMacro);
	void Release();

	VkShaderModule GetShader() const;
	std::string GetEntryName() const;
	std::string GetProfileName() const;
	std::vector<std::string> GetShaderDefines() const;
	size_t GetShaderHash() const;
	std::filesystem::path GetSourcePath() const;
	std::filesystem::path GetOutputPath() const;

	bool operator==(const UHShader& InShader);

private:
	bool Create(VkShaderModuleCreateInfo InCreateInfo);

	VkShaderModule Shader;
	size_t ShaderHash;

	std::string ShaderName;
	std::filesystem::path SourcePath;
	std::string EntryName;
	std::string ProfileName;
	std::vector<std::string> ShaderDefines;
	bool bIsMaterialShader;

	friend UHGraphic;
};