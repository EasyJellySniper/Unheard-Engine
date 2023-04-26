#include "TextureImporter.h"

#if WITH_DEBUG
#include "../Runtime/Classes/AssetPath.h"
#include <regex>

UHTextureImporter::UHTextureImporter()
{
	std::ifstream FileIn("GammaTextures.txt", std::ios::in);
	if (FileIn.is_open())
	{
		while (!FileIn.eof())
		{
			std::string GammaTex;
			std::getline(FileIn, GammaTex);
			GammaTextures.push_back(GammaTex);
		}
	}
	FileIn.close();
}

std::vector<uint8_t> UHTextureImporter::LoadTexture(std::wstring Filename, uint32_t& Width, uint32_t& Height)
{
	// the work flow is: creating factory->creating decoder->creating frame->get image info->allocate memory->copy
	Width = 0;
	Height = 0;

	ComPtr<IWICImagingFactory> WicFactory;
	RETURN_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&WicFactory)));

	ComPtr<IWICBitmapDecoder> Decoder;
	RETURN_IF_FAILED(WicFactory->CreateDecoderFromFilename(Filename.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, Decoder.GetAddressOf()));

	ComPtr<IWICBitmapFrameDecode> Frame;
	RETURN_IF_FAILED(Decoder->GetFrame(0, Frame.GetAddressOf()));

	RETURN_IF_FAILED(Frame->GetSize(&Width, &Height));

	WICPixelFormatGUID PixelFormat;
	RETURN_IF_FAILED(Frame->GetPixelFormat(&PixelFormat));

	uint32_t RowPitch = Width * sizeof(uint32_t);
	uint32_t TextureSize = RowPitch * Height;

	std::vector<uint8_t> Texture;
	Texture.resize(static_cast<size_t>(TextureSize));

	if (memcmp(&PixelFormat, &GUID_WICPixelFormat32bppRGBA, sizeof(GUID)) == 0)
	{
		// copy data directly if it's WIC format
		RETURN_IF_FAILED(Frame->CopyPixels(nullptr, RowPitch, TextureSize, reinterpret_cast<BYTE*>(Texture.data())));
	}
	else
	{
		// for other formats which aren't WIC, try to convert them
		ComPtr<IWICFormatConverter> FormatConverter;
		RETURN_IF_FAILED(WicFactory->CreateFormatConverter(FormatConverter.GetAddressOf()));

		BOOL CanConvert = FALSE;
		RETURN_IF_FAILED(FormatConverter->CanConvert(PixelFormat, GUID_WICPixelFormat32bppRGBA, &CanConvert));

		RETURN_IF_FAILED(FormatConverter->Initialize(Frame.Get(), GUID_WICPixelFormat32bppRGBA,
			WICBitmapDitherTypeErrorDiffusion, nullptr, 0, WICBitmapPaletteTypeMedianCut));

		RETURN_IF_FAILED(FormatConverter->CopyPixels(nullptr, RowPitch, TextureSize, reinterpret_cast<BYTE*>(Texture.data())));
	}

	return Texture;
}

bool UHTextureImporter::IsUHTextureCached(std::filesystem::path InPath)
{
	for (const UHRawTextureAssetCache& Cache : TextureAssetCaches)
	{
		if (std::filesystem::exists(Cache.SourcePath))
		{
			if (Cache.UHTexturePath == InPath)
			{
				return true;
			}
		}
	}

	return false;
}

bool UHTextureImporter::IsRawTextureCached(std::filesystem::path InPath)
{
	bool bResult = false;
	for (size_t Idx = 0; Idx < TextureAssetCaches.size() && !bResult; Idx++)
	{
		bool bIsPathEqual = TextureAssetCaches[Idx].SourcePath == InPath;
		bool bIsModifiedTimeEqual = TextureAssetCaches[Idx].SourcLastModifiedTime == std::filesystem::last_write_time(InPath).time_since_epoch().count();
		bool bIsUHTextureExist = std::filesystem::exists(TextureAssetCaches[Idx].UHTexturePath);

		// check path / modified time / UHTexture
		bResult = bIsPathEqual && bIsModifiedTimeEqual && bIsUHTextureExist;
	}

	return bResult;
}

void UHTextureImporter::AddRawTextureAssetCache(UHRawTextureAssetCache InAssetCache)
{
	// add non-duplicate cache to list
	if (!UHUtilities::FindByElement(TextureAssetCaches, InAssetCache))
	{
		TextureAssetCaches.push_back(InAssetCache);
	}
}

void UHTextureImporter::ImportRawTextures()
{
	if (!std::filesystem::exists(GRawTexturePath))
	{
		std::filesystem::create_directories(GRawTexturePath);
	}

	// load raw textures from raw asset folder, skip loading if it's already processed as UH texture
	for (std::filesystem::recursive_directory_iterator Idx(GRawTexturePath.c_str()), end; Idx != end; Idx++)
	{
		// check if it's image format
		const std::regex ImageFormats(".(jpg|jpeg|png|gif|bmp|BMP|JPG|JPEG|PNG|GIF)$");

		// skip directory and cached raw texture
		if (std::filesystem::is_directory(Idx->path()) || IsRawTextureCached(Idx->path())
			|| !std::regex_match(Idx->path().extension().string(), ImageFormats))
		{
			continue;
		}

		uint32_t Width, Height;
		std::vector<uint8_t> TextureData = LoadTexture(Idx->path().wstring(), Width, Height);
		if (Width == 0 && Height == 0)
		{
			UHE_LOG(L"Failed to load texture: " + Idx->path().filename().wstring() + L" !\n");
			continue;
		}

		VkExtent2D Extent;
		Extent.width = Width;
		Extent.height = Height;

		// output UH texture with original path structure
		std::string OriginSubpath = UHAssetPath::GetTextureOriginSubpath(Idx->path());

		// @TODO: setting linear/sRGB if I have an editor, this is just a silly temp method
		bool bIsLinear = true;
		for (const std::string& GammaTex : GammaTextures)
		{
			if (std::filesystem::path(GammaTex) == std::filesystem::path(OriginSubpath + Idx->path().stem().string()))
			{
				bIsLinear = false;
				break;
			}
		}

		VkFormat DesiredFormat = bIsLinear ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;
		UHTexture2D NewTex(Idx->path().stem().string(), OriginSubpath + Idx->path().stem().string(), Extent, DesiredFormat, bIsLinear);
		NewTex.SetTextureData(TextureData);
		NewTex.Export(GTextureAssetFolder + OriginSubpath);

		// add cache
		UHRawTextureAssetCache Cache{};
		Cache.SourcePath = Idx->path();
		Cache.SourcLastModifiedTime = std::filesystem::last_write_time(Idx->path()).time_since_epoch().count();
		Cache.UHTexturePath = GTextureAssetFolder + OriginSubpath + NewTex.GetName() + GTextureAssetExtension;

		AddRawTextureAssetCache(Cache);
	}

	OutputCaches();
}

// load caches
void UHTextureImporter::LoadCaches()
{
	// check cache folder
	if (!std::filesystem::exists(GTextureAssetCachePath))
	{
		std::filesystem::create_directories(GTextureAssetCachePath);
	}

	for (std::filesystem::recursive_directory_iterator Idx(GTextureAssetCachePath.c_str()), end; Idx != end; Idx++)
	{
		if (std::filesystem::is_directory(Idx->path()) || !UHAssetPath::IsTheSameExtension(Idx->path(), GTextureAssetCacheExtension))
		{
			continue;
		}

		std::ifstream FileIn(Idx->path().string().c_str(), std::ios::in | std::ios::binary);

		// load cache
		UHRawTextureAssetCache Cache;

		// load source path
		std::string TempString;
		UHUtilities::ReadStringData(FileIn, TempString);
		Cache.SourcePath = TempString;

		// load last modified time of source
		FileIn.read(reinterpret_cast<char*>(&Cache.SourcLastModifiedTime), sizeof(Cache.SourcLastModifiedTime));

		UHUtilities::ReadStringData(FileIn, TempString);
		Cache.UHTexturePath = TempString;

		FileIn.close();

		TextureAssetCaches.push_back(Cache);
	}
}

// output caches
void UHTextureImporter::OutputCaches()
{
	// open cache files
	for (const UHRawTextureAssetCache& Cache : TextureAssetCaches)
	{
		std::string CacheName = Cache.SourcePath.stem().string();
		std::string OriginSubpath = UHAssetPath::GetTextureOriginSubpath(Cache.SourcePath);

		// check cache folder
		if (!std::filesystem::exists(GTextureAssetCachePath + OriginSubpath))
		{
			std::filesystem::create_directories(GTextureAssetCachePath + OriginSubpath);
		}

		std::ofstream FileOut(GTextureAssetCachePath + OriginSubpath + CacheName + GTextureAssetCacheExtension, std::ios::out | std::ios::binary);

		// output necessary data
		UHUtilities::WriteStringData(FileOut, Cache.SourcePath.string());
		FileOut.write(reinterpret_cast<const char*>(&Cache.SourcLastModifiedTime), sizeof(&Cache.SourcLastModifiedTime));

		UHUtilities::WriteStringData(FileOut, Cache.UHTexturePath.string());

		FileOut.close();
	}
}
#endif