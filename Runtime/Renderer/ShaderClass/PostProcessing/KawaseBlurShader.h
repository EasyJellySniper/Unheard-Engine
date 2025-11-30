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

	void BindParameters(UHRenderBuilder& RenderBuilder, UHTexture* Input, UHTexture* Output);

	static const int32_t KawaseBlurCount = 4;
private:
	UHKawaseBlurType KawaseBlurType;
};

struct UHKawaseBlurConstants
{
	uint32_t Width;
	uint32_t Height;

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