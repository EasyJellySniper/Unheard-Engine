#include "TextureCompressor.h"

#if WITH_EDITOR
#include <assert.h>
#include "Types.h"
#include "Utility.h"
#define IMATH_HALF_NO_LOOKUP_TABLE
#include <ImfRgba.h>
#include "../Renderer/ShaderClass/BlockCompressionShader.h"
#include "../Renderer/RenderBuilder.h"

// compress raw texture data to block compression, implementation follows the Microsoft document
// https://learn.microsoft.com/en-us/windows/win32/direct3d10/d3d10-graphics-programming-guide-resources-block-compression
// input is assumed as RGBA8888 for non-HDR, and RGBAHalf for HDR
// implementation is done on the compute shader
namespace UHTextureCompressor
{
	struct UHCompressionConstant
	{
		UHCompressionConstant()
			: Width(0)
			, Height(0)
			, IsBC3(0)
		{
		}

		uint32_t Width;
		uint32_t Height;
		int32_t IsBC3;
	};

	void BlockCompressionColorGPU(const uint32_t Width, const uint32_t Height, std::vector<UHColorRGB>& RGB888, UHGraphic* InGfx, std::vector<uint64_t>& Output, bool bIsBC3)
	{
		// 8 bytes per 4x4 block, BC1 compression, alpha channel will be discard
		const uint32_t OutputSize = std::max(Width * Height / 16, (uint32_t)1);

		// prepare compression shader
		UniquePtr<UHBlockCompressionShader> ColorCompressor = MakeUnique<UHBlockCompressionShader>(InGfx, "ColorCompressor", "BlockCompressColor");
		UHRenderBuilder RenderBuilder(InGfx, InGfx->BeginOneTimeCmd());

		// prepare data buffer and bind
		UniquePtr<UHRenderBuffer<UHColorRGB>> InputData = InGfx->RequestRenderBuffer<UHColorRGB>(Width * Height, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
			, "TextureInputData");
		InputData->UploadAllData(RGB888.data());

		UniquePtr<UHRenderBuffer<uint64_t>> OutputData = InGfx->RequestRenderBuffer<uint64_t>(OutputSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
			, "TextureOutputData");
		UniquePtr<UHRenderBuffer<UHCompressionConstant>> Constants = InGfx->RequestRenderBuffer<UHCompressionConstant>(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
			, "TextureCompressionConstant");

		UHCompressionConstant Consts;
		Consts.Width = Width;
		Consts.Height = Height;
		Consts.IsBC3 = bIsBC3 ? 1 : 0;
		Constants->UploadData(&Consts, 0);

		ColorCompressor->BindStorage(OutputData.get(), 0, 0, true);
		ColorCompressor->BindStorage(InputData.get(), 1, 0, true);
		ColorCompressor->BindConstant(Constants, 2, 0, 0);

		// dispatch work
		RenderBuilder.BindComputeState(ColorCompressor->GetComputeState());
		RenderBuilder.BindDescriptorSetCompute(ColorCompressor->GetPipelineLayout(), ColorCompressor->GetDescriptorSet(0));
		if (Width * Height > 16)
		{
			RenderBuilder.Dispatch((Width + 4) / 4, (Height + 4) / 4, 1);
		}
		else
		{
			RenderBuilder.Dispatch(1, 1, 1);
		}
		InGfx->EndOneTimeCmd(RenderBuilder.GetCmdList());

		// readback data
		Output = OutputData->ReadbackData();

		ColorCompressor->Release();
		InputData->Release();
		OutputData->Release();
		Constants->Release();
	}

	void BlockCompressionAlphaGPU(const uint32_t Width, const uint32_t Height, std::vector<uint32_t>& Alpha8, UHGraphic* InGfx, std::vector<uint64_t>& Output)
	{
		// 8 bytes per 4x4 block, BC1 compression, alpha channel will be discard
		const uint32_t OutputSize = std::max(Width * Height / 16, (uint32_t)1);

		// prepare compression shader
		UniquePtr<UHBlockCompressionShader> AlphaCompressor = MakeUnique<UHBlockCompressionShader>(InGfx, "AlphaCompressor", "BlockCompressAlpha");
		UHRenderBuilder RenderBuilder(InGfx, InGfx->BeginOneTimeCmd());

		// prepare data buffer and bind
		UniquePtr<UHRenderBuffer<uint32_t>> InputData = InGfx->RequestRenderBuffer<uint32_t>(Width * Height, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
			, "TextureInputData");
		InputData->UploadAllData(Alpha8.data());

		UniquePtr<UHRenderBuffer<uint64_t>> OutputData = InGfx->RequestRenderBuffer<uint64_t>(OutputSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
			, "TextureOutputData");
		UniquePtr<UHRenderBuffer<UHCompressionConstant>> Constants = InGfx->RequestRenderBuffer<UHCompressionConstant>(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
			, "TextureCompressionConstant");

		UHCompressionConstant Consts;
		Consts.Width = Width;
		Consts.Height = Height;
		Consts.IsBC3 = 0;
		Constants->UploadData(&Consts, 0);

		AlphaCompressor->BindStorage(OutputData.get(), 0, 0, true);
		AlphaCompressor->BindStorage(InputData.get(), 1, 0, true);
		AlphaCompressor->BindConstant(Constants, 2, 0, 0);

		// dispatch work
		RenderBuilder.BindComputeState(AlphaCompressor->GetComputeState());
		RenderBuilder.BindDescriptorSetCompute(AlphaCompressor->GetPipelineLayout(), AlphaCompressor->GetDescriptorSet(0));
		if (Width * Height > 16)
		{
			RenderBuilder.Dispatch((Width + 4) / 4, (Height + 4) / 4, 1);
		}
		else
		{
			RenderBuilder.Dispatch(1, 1, 1);
		}
		InGfx->EndOneTimeCmd(RenderBuilder.GetCmdList());

		// readback data
		Output = OutputData->ReadbackData();

		AlphaCompressor->Release();
		InputData->Release();
		OutputData->Release();
		Constants->Release();
	}

	std::vector<uint64_t> CompressBC1(const uint32_t Width, const uint32_t Height, const std::vector<uint8_t>& Input, UHGraphic* InGfx)
	{
		// store color as float
		std::vector<UHColorRGB> RGB888(Width * Height);
		const size_t RawColorStride = 4;
		for (size_t Idx = 0; Idx < RGB888.size(); Idx++)
		{
			RGB888[Idx].R = (float)Input[Idx * RawColorStride];
			RGB888[Idx].G = (float)Input[Idx * RawColorStride + 1];
			RGB888[Idx].B = (float)Input[Idx * RawColorStride + 2];
		}

		std::vector<uint64_t> Output;
		BlockCompressionColorGPU(Width, Height, RGB888, InGfx, Output, false);
		return Output;
	}

	std::vector<uint64_t> CompressBC3(const uint32_t Width, const uint32_t Height, const std::vector<uint8_t>& Input, UHGraphic* InGfx)
	{
		// 16 bytes per 4x4 block, BC3 compression, alpha channel will be preserved
		const uint32_t OutputSize = std::max(Width * Height / 16, (uint32_t)1);
		std::vector<uint64_t> Output(OutputSize * 2);

		std::vector<UHColorRGB> RGB888(Width * Height);
		std::vector<uint32_t> Alpha8(Width * Height);

		const size_t RawColorStride = 4;
		for (size_t Idx = 0; Idx < RGB888.size(); Idx++)
		{
			RGB888[Idx].R = (float)Input[Idx * RawColorStride];
			RGB888[Idx].G = (float)Input[Idx * RawColorStride + 1];
			RGB888[Idx].B = (float)Input[Idx * RawColorStride + 2];
			Alpha8[Idx] = Input[Idx * RawColorStride + 3];
		}

		std::vector<uint64_t> AlphaOutput;
		BlockCompressionAlphaGPU(Width, Height, Alpha8, InGfx, AlphaOutput);

		std::vector<uint64_t> ColorOutput;
		BlockCompressionColorGPU(Width, Height, RGB888, InGfx, ColorOutput, true);

		// assign to the output
		for (size_t Idx = 0; Idx < ColorOutput.size(); Idx++)
		{
			Output[2 * Idx] = AlphaOutput[Idx];
			Output[2 * Idx + 1] = ColorOutput[Idx];
		}
		return Output;
	}

	std::vector<uint64_t> CompressBC4(const uint32_t Width, const uint32_t Height, const std::vector<uint8_t>& Input, UHGraphic* InGfx)
	{
		// 8 bytes per 4x4 block, BC4 compression, stores red channel only
		std::vector<uint32_t> Red8(Width * Height);
		const size_t RawColorStride = 4;
		for (size_t Idx = 0; Idx < Red8.size(); Idx++)
		{
			Red8[Idx] = Input[Idx * RawColorStride];
		}

		std::vector<uint64_t> AlphaOutput;
		BlockCompressionAlphaGPU(Width, Height, Red8, InGfx, AlphaOutput);
		return AlphaOutput;
	}

	std::vector<uint64_t> CompressBC5(const uint32_t Width, const uint32_t Height, const std::vector<uint8_t>& Input, UHGraphic* InGfx)
	{
		// 16 bytes per 4x4 block, BC5 compression, stores red/green channel only
		const uint32_t OutputSize = std::max(Width * Height / 16, (uint32_t)1);
		std::vector<uint64_t> Output(OutputSize * 2);

		std::vector<uint32_t> Red8(Width * Height);
		std::vector<uint32_t> Green8(Width * Height);
		const size_t RawColorStride = 4;
		for (size_t Idx = 0; Idx < Red8.size(); Idx++)
		{
			Red8[Idx] = Input[Idx * RawColorStride];
			Green8[Idx] = Input[Idx * RawColorStride + 1];
		}

		std::vector<uint64_t> RedOutput;
		BlockCompressionAlphaGPU(Width, Height, Red8, InGfx, RedOutput);

		std::vector<uint64_t> GreenOutput;
		BlockCompressionAlphaGPU(Width, Height, Green8, InGfx, GreenOutput);

		// assign output
		for (size_t Idx = 0; Idx < RedOutput.size(); Idx++)
		{
			Output[2 * Idx] = RedOutput[Idx];
			Output[2 * Idx + 1] = GreenOutput[Idx];
		}
		return Output;
	}

	void BlockCompressionHDRGPU(const uint32_t Width, const uint32_t Height, std::vector<UHColorRGBInt>& RGBInt, UHGraphic* InGfx, std::vector<uint64_t>& Output)
	{
		const uint32_t OutputSize = std::max(Width * Height / 16, (uint32_t)1);

		// prepare compression shader
		UniquePtr<UHBlockCompressionNewShader> BC6HCompressor = MakeUnique<UHBlockCompressionNewShader>(InGfx, "BC6HCompressor");
		UHRenderBuilder RenderBuilder(InGfx, InGfx->BeginOneTimeCmd());

		// prepare data buffer and bind
		UniquePtr<UHRenderBuffer<UHColorRGBInt>> InputData = InGfx->RequestRenderBuffer<UHColorRGBInt>(Width * Height, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
			, "TextureInputData");
		InputData->UploadAllData(RGBInt.data());

		UniquePtr<UHRenderBuffer<uint64_t>> OutputData = InGfx->RequestRenderBuffer<uint64_t>(OutputSize * 2, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
			, "TextureOutputData");
		UniquePtr<UHRenderBuffer<UHCompressionConstant>> Constants = InGfx->RequestRenderBuffer<UHCompressionConstant>(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
			, "TextureCompressionConstant");
		UHCompressionConstant Consts;
		Consts.Width = Width;
		Consts.Height = Height;
		Constants->UploadData(&Consts, 0);

		BC6HCompressor->BindStorage(OutputData.get(), 0, 0, true);
		BC6HCompressor->BindStorage(InputData.get(), 1, 0, true);
		BC6HCompressor->BindConstant(Constants, 2, 0, 0);

		// dispatch work
		RenderBuilder.BindComputeState(BC6HCompressor->GetComputeState());
		RenderBuilder.BindDescriptorSetCompute(BC6HCompressor->GetPipelineLayout(), BC6HCompressor->GetDescriptorSet(0));
		if (Width * Height > 16)
		{
			RenderBuilder.Dispatch((Width + 4) / 4, (Height + 4) / 4, 1);
		}
		else
		{
			RenderBuilder.Dispatch(1, 1, 1);
		}
		InGfx->EndOneTimeCmd(RenderBuilder.GetCmdList());

		// readback data
		Output = OutputData->ReadbackData();

		BC6HCompressor->Release();
		InputData->Release();
		OutputData->Release();
		Constants->Release();
	}

	std::vector<uint64_t> CompressBC6H(const uint32_t Width, const uint32_t Height, const std::vector<uint8_t>& Input, UHGraphic* InGfx)
	{
		// collect RGB info
		std::vector<UHColorRGBInt> RGBInt(Width * Height);

		// store float data
		size_t Stride = sizeof(Imf::Rgba);
		for (size_t Idx = 0; Idx < RGBInt.size(); Idx++)
		{
			Imf::Rgba RGBAHalf{};
			memcpy_s(&RGBAHalf, Stride, Input.data() + Idx * Stride, Stride);

			RGBInt[Idx].R = RGBAHalf.r.bits();
			RGBInt[Idx].G = RGBAHalf.g.bits();
			RGBInt[Idx].B = RGBAHalf.b.bits();
		}

		std::vector<uint64_t> Output;
		BlockCompressionHDRGPU(Width, Height, RGBInt, InGfx, Output);
		return Output;
	}
}
#endif