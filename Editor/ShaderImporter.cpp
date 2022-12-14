#include "ShaderImporter.h"

#if WITH_DEBUG
#include "../Runtime/Classes/Utility.h"
#include <assert.h>
#include "../Runtime/Classes/AssetPath.h"

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
		size_t NumDefines;
		FileIn.read(reinterpret_cast<char*>(&NumDefines), sizeof(NumDefines));

		Cache.Defines.resize(NumDefines);
		for (size_t Idx = 0; Idx < NumDefines; Idx++)
		{
			UHUtilities::ReadStringData(FileIn, Cache.Defines[Idx]);
		}

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

	// write defines
	size_t NumDefines = Defines.size();
	FileOut.write(reinterpret_cast<const char*>(&NumDefines), sizeof(NumDefines));
	for (size_t Idx = 0; Idx < NumDefines; Idx++)
	{
		UHUtilities::WriteStringData(FileOut, Defines[Idx]);
	}

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
	CreateProcessA("dxc_2022_07_18/bin/x64/dxc.exe",   // the exe path, got from DXC GitHub
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

	if (ErrorCount == 0)
	{
		UHE_LOG(L"Compile succeed!!\n");
	}
	else
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

	// compile, setup dx layout as well
	std::string CompileCmd = " -spirv -T " + ProfileName + " -E " + EntryName + " "
		+ std::filesystem::absolute(InSource).string()
		+ " -Fo " + std::filesystem::absolute(OutputShaderPath).string()
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

#endif