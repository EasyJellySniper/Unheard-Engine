#include "ShaderImporter.h"

#if WITH_EDITOR
#include "../../Runtime/Classes/Utility.h"
#include <assert.h>
#include "../../Runtime/Classes/AssetPath.h"
#include "../../Runtime/Classes/Material.h"
#include <sstream>

UHShaderImporter::UHShaderImporter()
{
	ShaderIncludes.push_back("UHInputs.hlsli");
	ShaderIncludes.push_back("UHCommon.hlsli");
	ShaderIncludes.push_back("RayTracing/UHRTCommon.hlsli");
}

void UHShaderImporter::LoadShaderCache()
{
	if (!std::filesystem::exists(GRawShaderCachePath))
	{
		std::filesystem::create_directories(GRawShaderCachePath);
	}

	// load every shader cache under the folder
	for (std::filesystem::recursive_directory_iterator Idx(GRawShaderCachePath.c_str()), end; Idx != end; Idx++)
	{
		if (std::filesystem::is_directory(Idx->path()) || !UHAssetPath::IsTheSameExtension(Idx->path(), GShaderAssetCacheExtension))
		{
			continue;
		}

		std::ifstream FileIn(Idx->path().string().c_str(), std::ios::in | std::ios::binary);

		// load cache
		UHRawShaderAssetCache Cache;

		// load source path
		std::string TempString;
		UHUtilities::ReadStringData(FileIn, TempString);
		Cache.SourcePath = TempString;

		// load last modified time of source
		FileIn.read(reinterpret_cast<char*>(&Cache.SourcLastModifiedTime), sizeof(Cache.SourcLastModifiedTime));

		// load UHShaders path
		UHUtilities::ReadStringData(FileIn, TempString);
		Cache.UHShaderPath = TempString;

		// load entry name
		UHUtilities::ReadStringData(FileIn, TempString);
		Cache.EntryName = TempString;

		// load profile name
		UHUtilities::ReadStringData(FileIn, TempString);
		Cache.ProfileName = TempString;

		// load shader defines
		UHUtilities::ReadStringVectorData(FileIn, Cache.Defines);

		FileIn.close();

		UHRawShadersCache.push_back(Cache);
	}
}

void WriteShaderCache(std::filesystem::path SourcePath, std::filesystem::path UHShaderPath, std::string EntryName, std::string ProfileName
	, std::vector<std::string> Defines = std::vector<std::string>())
{
	// find origin path and try to preserve file structure
	std::string OriginSubpath = UHAssetPath::GetShaderOriginSubpath(SourcePath);

	// output shader cache with source file name + macro name
	size_t MacroHash = UHUtilities::ShaderDefinesToHash(Defines);
	std::string MacroHashName = (MacroHash != 0) ? "_" + std::to_string(MacroHash) : "";
	std::string CacheName = SourcePath.stem().string();

	if (!std::filesystem::exists(GRawShaderCachePath + OriginSubpath))
	{
		std::filesystem::create_directories(GRawShaderCachePath + OriginSubpath);
	}

	std::ofstream FileOut(GRawShaderCachePath + OriginSubpath + CacheName + EntryName + MacroHashName + GShaderAssetCacheExtension, std::ios::out | std::ios::binary);

	// get last modified time
	int64_t SourcLastModifiedTime = std::filesystem::last_write_time(SourcePath).time_since_epoch().count();

	UHUtilities::WriteStringData(FileOut, SourcePath.string());
	FileOut.write(reinterpret_cast<const char*>(&SourcLastModifiedTime), sizeof(SourcLastModifiedTime));
	UHUtilities::WriteStringData(FileOut, UHShaderPath.string());
	UHUtilities::WriteStringData(FileOut, EntryName);
	UHUtilities::WriteStringData(FileOut, ProfileName);
	UHUtilities::WriteStringVectorData(FileOut, Defines);

	FileOut.close();
}

void UHShaderImporter::WriteShaderIncludeCache()
{
	if (!std::filesystem::exists(GRawShaderCachePath))
	{
		std::filesystem::create_directories(GRawShaderCachePath);
	}

	// if shader includes are renamed, system will consider it's never cached and recompile every shaders.
	for (const std::string& Include : ShaderIncludes)
	{
		std::filesystem::path Path = GRawShaderPath + Include;
		WriteShaderCache(Path, "", "", "");
	}
}

bool UHShaderImporter::IsShaderIncludeCached()
{
	bool bIsCached = true;
	for(const std::string& Include : ShaderIncludes)
	{
		// mark if shader includes are changed
		std::filesystem::path IncludeFilePath = GRawShaderPath + Include;
		UHRawShaderAssetCache InCache{};
		InCache.SourcePath = IncludeFilePath;
		InCache.SourcLastModifiedTime = std::filesystem::last_write_time(IncludeFilePath).time_since_epoch().count();

		bIsCached &= UHUtilities::FindByElement(UHRawShadersCache, InCache);
	}

	return bIsCached;
}

bool UHShaderImporter::IsShaderCached(std::filesystem::path SourcePath, std::filesystem::path UHShaderPath, std::string EntryName, std::string ProfileName
	, std::vector<std::string> Defines)
{
	if (!std::filesystem::exists(SourcePath))
	{
		return false;
	}

	UHRawShaderAssetCache InCache;
	InCache.SourcePath = SourcePath;
	InCache.UHShaderPath = UHShaderPath;
	InCache.SourcLastModifiedTime = std::filesystem::last_write_time(SourcePath).time_since_epoch().count();
	InCache.EntryName = EntryName;
	InCache.ProfileName = ProfileName;
	InCache.Defines = Defines;

	// the cache will also check the include files
	return UHUtilities::FindByElement(UHRawShadersCache, InCache) && std::filesystem::exists(UHShaderPath) && IsShaderIncludeCached();
}

bool UHShaderImporter::IsShaderTemplateCached(std::filesystem::path SourcePath, std::string EntryName, std::string ProfileName)
{
	if (!std::filesystem::exists(SourcePath))
	{
		return false;
	}

	// less info than regular shader
	UHRawShaderAssetCache InCache{};
	InCache.SourcePath = SourcePath;
	InCache.SourcLastModifiedTime = std::filesystem::last_write_time(SourcePath).time_since_epoch().count();
	InCache.EntryName = EntryName;
	InCache.ProfileName = ProfileName;

	return UHUtilities::FindByElement(UHRawShadersCache, InCache) && IsShaderIncludeCached();
}

bool CompileShader(std::string CommandLine)
{
	HANDLE Std_OUT_Rd = NULL;
	HANDLE Std_OUT_Wr = NULL;

	// additional information
	STARTUPINFOA StartInfo;
	PROCESS_INFORMATION ProcInfo;
	SECURITY_ATTRIBUTES SecurityAttr;

	ZeroMemory(&SecurityAttr, sizeof(SecurityAttr));
	SecurityAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	SecurityAttr.bInheritHandle = TRUE;
	SecurityAttr.lpSecurityDescriptor = NULL;

	// Create a pipe for the child process's STDOUT. 
	CreatePipe(&Std_OUT_Rd, &Std_OUT_Wr, &SecurityAttr, 0);

	// Ensure the read handle to the pipe for STDOUT is not inherited.
	SetHandleInformation(Std_OUT_Rd, HANDLE_FLAG_INHERIT, 0);

	// set the size of the structures
	ZeroMemory(&StartInfo, sizeof(StartInfo));
	StartInfo.cb = sizeof(StartInfo);
	StartInfo.hStdError = Std_OUT_Wr;
	StartInfo.hStdOutput = Std_OUT_Wr;
	StartInfo.dwFlags |= STARTF_USESTDHANDLES;

	ZeroMemory(&ProcInfo, sizeof(ProcInfo));

	// start the program up
	CreateProcessA("ThirdParty/DirectXShaderCompiler/bin/x64/dxc.exe",   // the exe path, got from DXC GitHub
		const_cast<char*>(CommandLine.c_str()),        // Command line
		NULL,           // Process handle not inheritable
		NULL,           // Thread handle not inheritable
		TRUE,          // Set handle inheritance to true so I can read std output
		CREATE_NO_WINDOW,              // hide console
		NULL,           // Use parent's environment block
		NULL,           // Use parent's starting directory 
		&StartInfo,            // Pointer to STARTUPINFO structure
		&ProcInfo             // Pointer to PROCESS_INFORMATION structure (removed extra parentheses)
	);
	CloseHandle(Std_OUT_Wr);

	DWORD DwRead;
	CHAR Buff[2048];
	Buff[2047] = '\0';
	BOOL bSuccess = FALSE;

	int32_t ErrorCount = 0;
	while (true)
	{
		memset(Buff, 0, 2048);
		bSuccess = ReadFile(Std_OUT_Rd, Buff, 2048, &DwRead, NULL);
		if (!bSuccess || DwRead == 0)
		{
			break;
		}

		// Log chBuf
		UHE_LOG(UHUtilities::ToStringW(Buff));
		ErrorCount++;
	}

	// Close process and thread handles. 
	CloseHandle(ProcInfo.hProcess);
	CloseHandle(ProcInfo.hThread);
	CloseHandle(Std_OUT_Rd);

	if (ErrorCount != 0)
	{
		assert(("Shader compilation failed, see output logs for details!", ErrorCount == 0));
		return false;
	}

	return true;
}

void UHShaderImporter::CompileHLSL(std::string InShaderName, std::filesystem::path InSource, std::string EntryName, std::string ProfileName
	, std::vector<std::string> Defines)
{
	// compile HLSL shader by calling dxc.exe, which supports Vulkan spir-v
	// after it compiled succeed, export it to UH asset folder
	//dxc.exe -spirv -T <target-prfile> -E <entry-point> <hlsl - src - file> -Fo <spirv - bin - file>


	// find origin path and try to preserve file structure
	std::string OriginSubpath = UHAssetPath::GetShaderOriginSubpath(InSource);

	// get macro hash name and setup output file name
	size_t MacroHash = UHUtilities::ShaderDefinesToHash(Defines);
	std::string MacroHashName = (MacroHash != 0) ? "_" + std::to_string(MacroHash) : "";
	std::filesystem::path OutputShaderPath = GShaderAssetFolder + OriginSubpath + InShaderName + MacroHashName + GShaderAssetExtension;

	if (!std::filesystem::exists(GShaderAssetFolder + OriginSubpath))
	{
		std::filesystem::create_directories(GShaderAssetFolder + OriginSubpath);
	}

	// check if shader is cached
	if (IsShaderCached(InSource, OutputShaderPath, EntryName, ProfileName, Defines))
	{
		return;
	}

	// compile, setup dx layout as well, be careful that the path must include "" mark in case there are folders with whitespace name
	const std::string QuoteMark = "\"";
	std::string CompileCmd = " -spirv -T " + ProfileName + " -E " + EntryName + " "
		+ QuoteMark + std::filesystem::absolute(InSource).string() + QuoteMark
		+ " -Fo " + QuoteMark + std::filesystem::absolute(OutputShaderPath).string() + QuoteMark
		+ " -fvk-use-dx-layout "
		+ " -fvk-use-dx-position-w "
		+ " -fspv-target-env=vulkan1.1spirv1.4 ";

	// add define command lines
	for (const std::string& Define : Defines)
	{
		CompileCmd += " -D " + Define;
	}

	UHE_LOG(L"Compiling " + InSource.wstring() + L"...\n");
	bool bCompileResult = CompileShader(CompileCmd);

	if (!std::filesystem::exists(OutputShaderPath) || !bCompileResult)
	{
		UHE_LOG(L"Failed to compile shader " + InSource.wstring() + L"!\n");
		return;
	}

	// write shader cache
	WriteShaderCache(InSource, OutputShaderPath, EntryName, ProfileName, Defines);
}

std::string UHShaderImporter::TranslateHLSL(std::string InShaderName, std::filesystem::path InSource, std::string EntryName, std::string ProfileName, UHMaterialCompileData InData
	, std::vector<std::string> Defines)
{
	// Workflow of translate HLSL
	// 1) Load shader template as string
	// 2) Replace the code to the marker
	// 3) Save it as temporary file
	// 4) Compile it and write cache, force write should be fine, compile should be happen when editing in material editor

	std::string ShaderCode;
	std::ifstream FileIn(std::filesystem::absolute(InSource).string().c_str(), std::ios::in);
	if (FileIn.is_open())
	{
		std::stringstream Buffer;
		Buffer << FileIn.rdbuf();
		ShaderCode = Buffer.str();
	}
	FileIn.close();

	if (ShaderCode.empty())
	{
		// failed to translate HLSL due to template not found
		UHE_LOG("Shader template " + InShaderName + " not found.\n");
		return "";
	}

	// calculate and set material buffer size for non hit group shader
	if (!InData.bIsHitGroup)
	{
		size_t MaterialBufferSize = 0;
		ShaderCode = UHUtilities::StringReplace(ShaderCode, "//%UHS_CBUFFERDEFINE", InData.MaterialCache->GetCBufferDefineCode(MaterialBufferSize));
		InData.MaterialCache->SetMaterialBufferSize(MaterialBufferSize);
	}

	// get source code returned from material, and replace it in template code
	const std::vector<std::string> ShaderInputIdentifiers = { "//%UHS_INPUT"
		, "//%UHS_INPUT_OpacityOnly"
		, "//%UHS_INPUT_OpacityNormalRough"
		, "//%UHS_INPUT_NormalOnly"
		, "//%UHS_INPUT_EmissiveOnly"};

	for (int32_t Idx = UH_ENUM_VALUE(UHMaterialInputType::MaterialInputMax) - 1; Idx >= 0 ; Idx--)
	{
		InData.InputType = static_cast<UHMaterialInputType>(Idx);
		ShaderCode = UHUtilities::StringReplace(ShaderCode, ShaderInputIdentifiers[Idx], InData.MaterialCache->GetMaterialInputCode(InData));
	}


	if (!std::filesystem::exists(GTempFilePath))
	{
		std::filesystem::create_directories(GTempFilePath);
	}

	if (!std::filesystem::exists(GShaderAssetFolder))
	{
		std::filesystem::create_directories(GShaderAssetFolder);
	}

	// macro hash
	const size_t MacroHash = UHUtilities::ShaderDefinesToHash(Defines);
	const std::string MacroHashName = (MacroHash != 0) ? "_" + std::to_string(MacroHash) : "";
	const std::string OutName = UHAssetPath::FormatMaterialShaderOutputPath("", InData.MaterialCache->GetSourcePath(), InShaderName, MacroHashName);

	// output temp shader file
	const std::string TempShaderPath = GTempFilePath + OutName;

	std::ofstream FileOut((TempShaderPath + GRawShaderExtension).c_str(), std::ios::out);
	FileOut << ShaderCode;
	FileOut.close();

	// deside the output shader path based on the compile flag
	// Hit compile but not save it, it shall not refresh the regular spv file
	// regular spv file should be also refreshed when include changes
	std::string OutputShaderPath = (InData.MaterialCache->GetCompileFlag() == UHMaterialCompileFlag::FullCompileResave || InData.MaterialCache->GetCompileFlag() == UHMaterialCompileFlag::IncludeChanged)
		? GShaderAssetFolder + OutName + GShaderAssetExtension
		: TempShaderPath + GShaderAssetExtension;

	// compile, setup dx layout as well, be careful with path system, quotation mark is needed
	const std::string QuoteMark = "\"";
	std::string CompileCmd = " -spirv -T " + ProfileName + " -E " + EntryName + " "
		+ QuoteMark + std::filesystem::absolute(TempShaderPath + GRawShaderExtension).string() + QuoteMark
		+ " -Fo " + QuoteMark + std::filesystem::absolute(OutputShaderPath).string() + QuoteMark
		+ " -fvk-use-dx-layout "
		+ " -fvk-use-dx-position-w "
		+ " -fspv-target-env=vulkan1.1spirv1.4 ";

	// add define command lines
	for (const std::string& Define : Defines)
	{
		CompileCmd += " -D " + Define;
	}

	UHE_LOG("Compiling " + OutputShaderPath + "...\n");
	bool bCompileResult = CompileShader(CompileCmd);

	if (!std::filesystem::exists(OutputShaderPath) || !bCompileResult)
	{
		UHE_LOG("Failed to compile shader " + OutputShaderPath + "!\n");
		return "";
	}

	// write shader cache, the template doesn't need macro and output shader path
	WriteShaderCache(InSource, "", EntryName, ProfileName);

	// return the output shader path
	return OutputShaderPath;
}

#endif