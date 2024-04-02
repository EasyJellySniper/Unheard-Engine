#include "TextureImporter.h"

#if WITH_EDITOR
#include "../../Runtime/Classes/AssetPath.h"
#include "../../../Runtime/Engine/Graphic.h"
#define IMATH_HALF_NO_LOOKUP_TABLE
#include <ImfRgbaFile.h>
#include <ImfArray.h>

UHTextureImporter::UHTextureImporter()
	: Gfx(nullptr)
{

}

UHTextureImporter::UHTextureImporter(UHGraphic* InGraphic)
	: Gfx(InGraphic)
{

}

std::vector<uint8_t> UHTextureImporter::LoadTexture(std::filesystem::path Filename, uint32_t& Width, uint32_t& Height)
{
	std::vector<uint8_t> Texture;
	std::filesystem::path FilePath = Filename;
	const bool bIsEXR = FilePath.extension().string() == ".exr";

	if (bIsEXR)
	{
		// Load EXR file
		Imf::RgbaInputFile File(Filename.string().c_str());
		Imath::Box2i Dimension = File.dataWindow();
		Width = Dimension.max.x - Dimension.min.x + 1;
		Height = Dimension.max.y - Dimension.min.y + 1;

		Imf::Array2D<Imf::Rgba> Pixels(Width, Height);
		File.setFrameBuffer(&Pixels[0][0], 1, Width);
		File.readPixels(Dimension.min.y, Dimension.max.y);

		// check the float value, and relatively scale down with the max value searched
		float HalfMax = 65504.0f;
		float MaxValueR = 0.0f;
		float MaxValueG = 0.0f;
		float MaxValueB = 0.0f;

		for (uint32_t X = 0; X < Width; X++)
		{
			for (uint32_t Y = 0; Y < Height; Y++)
			{
				Imf::Rgba& Color = Pixels[X][Y];
				UHColorRGB ColorF(Color.r, Color.g, Color.b);
				if (isinf(ColorF.R))
				{
					ColorF.R = HalfMax;
					Color.r = ColorF.R;
				}

				if (isinf(ColorF.G))
				{
					ColorF.G = HalfMax;
					Color.g = ColorF.G;
				}

				if (isinf(ColorF.B))
				{
					ColorF.B = HalfMax;
					Color.b = ColorF.B;
				}

				MaxValueR = (std::max)(MaxValueR, ColorF.R);
				MaxValueG = (std::max)(MaxValueG, ColorF.G);
				MaxValueB = (std::max)(MaxValueB, ColorF.B);
			}
		}

		// scale down if max value of color exceed the desired max value
		const float DesiredMaxValue = 32767.0f;
		if (MaxValueR > DesiredMaxValue || MaxValueG > DesiredMaxValue || MaxValueB > DesiredMaxValue)
		{
			for (uint32_t X = 0; X < Width; X++)
			{
				for (uint32_t Y = 0; Y < Height; Y++)
				{
					Imf::Rgba& Color = Pixels[X][Y];
					UHColorRGB ColorF(Color.r, Color.g, Color.b);

					Color.r = ColorF.R / MaxValueR * DesiredMaxValue;
					Color.g = ColorF.G / MaxValueG * DesiredMaxValue;
					Color.b = ColorF.B / MaxValueB * DesiredMaxValue;
				}
			}
		}

		// allocate W * H * sizeof(R16G16B16A16)
		size_t Size = Width * Height * sizeof(Imf::Rgba);
		Texture.resize(Size);
		memcpy_s(&Texture[0], Size, &Pixels[0][0], Size);
	}
	else
	{
		// BMP, JPG, PNG
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
	}

	return Texture;
}

void CompressionSettingToFormat(const UHTextureSettings& InSetting, UHTextureFormat& OutFormat)
{
	switch (InSetting.CompressionSetting)
	{
	case BC1:
		OutFormat = (InSetting.bIsLinear) ? UH_FORMAT_BC1_UNORM : UH_FORMAT_BC1_SRGB;
		break;

	case BC3:
		OutFormat = (InSetting.bIsLinear) ? UH_FORMAT_BC3_UNORM : UH_FORMAT_BC3_SRGB;
		break;

	case BC4:
		OutFormat = UH_FORMAT_BC4;
		break;

	case BC5:
		OutFormat = UH_FORMAT_BC5;
		break;

	case BC6H:
		OutFormat = UH_FORMAT_BC6H;
		break;
	}
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

	UHTextureFormat DesiredFormat = InSettings.bIsLinear ? UH_FORMAT_RGBA8_UNORM : UH_FORMAT_RGBA8_SRGB;
	DesiredFormat = InSettings.bIsHDR ? UH_FORMAT_RGBA16F : DesiredFormat;
	CompressionSettingToFormat(InSettings, DesiredFormat);

	std::string OutputPathName = OutputFolder.string() + "/" + SourcePath.stem().string();
	std::string SavedPathName = UHUtilities::StringReplace(OutputPathName, "\\", "/");
	SavedPathName = UHUtilities::StringReplace(SavedPathName, GTextureAssetFolder, "");

	UniquePtr<UHTexture2D> NewTex = MakeUnique<UHTexture2D>(SourcePath.stem().string(), SavedPathName, Extent, DesiredFormat, InSettings);
	UHTexture2D* OutputTex = Gfx->RequestTexture2D(NewTex, true);
	OutputTex->SetRawSourcePath(SourcePath.string());
	OutputTex->SetTextureData(TextureData);

	if (OutputTex != nullptr)
	{
		OutputTex->Recreate(true);
		OutputTex->Export(OutputPathName);
		MessageBoxA(nullptr, ("Import " + OutputTex->GetName() + " successfully.").c_str(), "Texture Import", MB_OK);
	}

	return OutputTex;
}

#endif