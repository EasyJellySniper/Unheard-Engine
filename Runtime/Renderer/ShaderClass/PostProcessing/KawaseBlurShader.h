#pragma once
#include "../ShaderClass.h"

enum class UHKawaseBlurType
{
	Downsample = 0,
	Upsample
};

class UHKawaseBlurShader : public UHShaderClass
{
public:
	UHKawaseBlurShader(UHGraphic* InGfx, std::string Name, UHKawaseBlurType InType);
	virtual void OnCompile() override;

	void BindParameters(UHRenderBuilder& RenderBuilder, UHTexture* Input, UHTexture* Output
		, uint32_t InputMip = 0, uint32_t OutputMip = 0);
private:
	UHKawaseBlurType KawaseBlurType;
};

struct UHKawaseBlurConstants
{
	UHKawaseBlurConstants()
		: Width(0)
		, Height(0)
		, PassCount(0)
		, bUseMipAsTempRT(false)
		, StartInputMip(0)
		, StartOutputMip(0)
		, bDownsampleOnly(false)
		, PreserveAlpha(0)
	{
	}

	uint32_t Width;
	uint32_t Height;
	uint32_t PreserveAlpha;
	int32_t PassCount;
	bool bUseMipAsTempRT;
	int32_t StartInputMip;
	int32_t StartOutputMip;
	bool bDownsampleOnly;

	void Release(UHGraphic* InGfx)
	{
		for (size_t Idx = 0; Idx < KawaseTempRT.size(); Idx++)
		{
			if (KawaseTempRT[Idx] != nullptr)
			{
				InGfx->RequestReleaseRT(KawaseTempRT[Idx]);
			}
		}

		KawaseTempRT.clear();
	}

	// extra temp RTs are needed for Kawase blur, depends on how many passes are applied
	std::vector<UHRenderTexture*> KawaseTempRT;
};