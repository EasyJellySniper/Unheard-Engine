#include "TextureImporter.h"

#if WITH_DEBUG
#include "../Runtime/Classes/AssetPath.h"
#include "../../Runtime/Engine/Graphic.h"

UHTextureImporter::UHTextureImporter()
	: Gfx(nullptr)
{

}

UHTextureImporter::UHTextureImporter(UHGraphic* InGraphic)
	: Gfx(InGraphic)
{

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

// import raw texture from a specified path, this should be called after something like open file dialog
UHTexture* UHTextureImporter::ImportRawTexture(std::filesystem::path SourcePath, std::filesystem::path OutputFolder, UHTextureSettings InSettings)
{
	if (!std::filesystem::exists(SourcePath))
	{
		UHE_LOG("Texture file doesn't exist!\n");
		return nullptr;
	}

	uint32_t Width, Height;
	std::vector<uint8_t> TextureData = LoadTexture(SourcePath.wstring(), Width, Height);
	if (Width == 0 && Height == 0)
	{
		UHE_LOG(L"Failed to load texture: " + SourcePath.wstring() + L" !\n");
		return nullptr;
	}

	VkExtent2D Extent;
	Extent.width = Width;
	Extent.height = Height;

	VkFormat DesiredFormat = InSettings.bIsLinear ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;
	std::string OutputPathName = OutputFolder.string() + "/" + SourcePath.stem().string();
	std::string SavedPathName = UHUtilities::StringReplace(OutputPathName, "\\", "/");
	SavedPathName = UHUtilities::StringReplace(SavedPathName, GTextureAssetFolder, "");

	UHTexture2D NewTex(SourcePath.stem().string(), SavedPathName, Extent, DesiredFormat, InSettings);
	NewTex.SetTextureData(TextureData);

	UHTexture2D* OutputTex = Gfx->RequestTexture2D(NewTex);
	OutputTex->SetRawSourcePath(SourcePath.string());
	if (OutputTex != nullptr)
	{
		OutputTex->Recreate();
		OutputTex->Export(OutputPathName);
		MessageBoxA(nullptr, ("Import " + NewTex.GetName() + " successfully.").c_str(), "Texture Import", MB_OK);
	}

	return OutputTex;
}

#endif