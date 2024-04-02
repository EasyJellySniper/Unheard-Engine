#include "TextureNode.h"
#include "../Utility.h"
#include "../MaterialLayout.h"
#include "../../Engine/Asset.h"

UHTexture2DNode::UHTexture2DNode(std::string TexName)
	: UHGraphNode(true)
{
	Name = "Texture2D";
	NodeType = Texture2DNode;
	TextureIndexInMaterial = UHINDEXNONE;

	Inputs.resize(1);
	Inputs[0] = MakeUnique<UHGraphPin>("UV", this, Float2Pin);

	Outputs.resize(4);
	Outputs[0] = MakeUnique<UHGraphPin>("RGB", this, Float3Pin);
	Outputs[1] = MakeUnique<UHGraphPin>("R", this, FloatPin);
	Outputs[2] = MakeUnique<UHGraphPin>("G", this, FloatPin);
	Outputs[3] = MakeUnique<UHGraphPin>("B", this, FloatPin);

	SelectedTexturePathName = TexName;
}

bool UHTexture2DNode::CanEvalHLSL()
{
	if (SelectedTexturePathName.empty())
	{
		return false;
	}

	return true;
}

std::string UHTexture2DNode::EvalDefinition()
{
#if WITH_EDITOR
	// Eval local definition for texture sample, so it will only be sampled once only
	// this also considers the bindless rendering
	// float4 Result_1234 = UHTextureTable[Node_1234_Index].Sample(Sampler_Index, UV);
	// don't worry about texture index definition, that's handled by material class
	if (CanEvalHLSL())
	{
		const std::string IDString = std::to_string(GetId());
		std::string UVString = GDefaultTextureChannel0Name;
		if (Inputs[0]->GetSrcPin() && Inputs[0]->GetSrcPin()->GetOriginNode())
		{
			UVString = Inputs[0]->GetSrcPin()->GetOriginNode()->EvalHLSL(Inputs[0]->GetSrcPin());
		}

		// consider if this is a bump texture and insert decode
		const UHTexture2D* Texture = UHAssetManager::GetTexture2DByPathEditor(SelectedTexturePathName);
		const UHTextureSettings& TextureSettings = Texture->GetTextureSettings();

		// if it's compiling for ray tracing, I need to use the SampleLevel instead of Sample
		if (bIsCompilingRayTracing)
		{
			// @TODO: Custom sampler index?
			const std::string TextureIndexCode = "MatData.Data[" + std::to_string(/*2 */ TextureIndexInMaterial + GRTMaterialDataStartIndex) + "]";
			const std::string SamplerIndexCode = GDefaultSamplerIndexName;
			//const std::string SamplerIndexCode = "MatData.Data[" + std::to_string(2 * TextureIndexInMaterial + 1 + GRTMaterialDataStartIndex) + "]";

			// the mip level will be calculated in the shader
			std::string Result =
				"float4 Result_" + IDString + " = UHTextureTable[" + TextureIndexCode + "].SampleLevel(UHSamplerTable[" + SamplerIndexCode + "], " + UVString + ", MipLevel);\n";

			if (TextureSettings.bIsNormal)
			{
				// insert decode bump normal code if it's normal map
				const std::string IsBC5 = TextureSettings.CompressionSetting == BC5 ? "true" : "false";
				Result += "\tResult_" + IDString + ".xyz = DecodeNormal(" + "Result_" + IDString + ".xyz, " + IsBC5 + ");\n";
			}

			return Result;
		}

		const std::string TextureIndexCode = "Node_" + IDString + "_Index";
		// @TODO: Custom sampler index?
		const std::string SamplerIndexCode = GDefaultSamplerIndexName;
		std::string Result = "float4 Result_" + IDString + " = UHTextureTable[" + TextureIndexCode + "].Sample(UHSamplerTable[" + SamplerIndexCode + "], " + UVString + ");\n";

		if (TextureSettings.bIsNormal)
		{
			// insert decode bump normal code if it's normal map
			const std::string IsBC5 = TextureSettings.CompressionSetting == BC5 ? "true" : "false";
			Result += "\tResult_" + IDString + ".xyz = DecodeNormal(" + "Result_" + IDString + ".xyz, " + IsBC5 + ");\n";
		}
		return Result;
	}
#endif
	return "[ERROR] Texture not set.";
}

std::string UHTexture2DNode::EvalHLSL(const UHGraphPin* CallerPin)
{
	if (CanEvalHLSL())
	{
		const int32_t ChannelIdx = FindOutputPinIndexInternal(CallerPin);
		const std::string IDString = std::to_string(GetId());
		return "Result_" + IDString + GOutChannelShared[ChannelIdx];
	}

	return "[ERROR] Texture not set.";
}

void UHTexture2DNode::InputData(std::ifstream& FileIn)
{
	UHUtilities::ReadStringData(FileIn, SelectedTexturePathName);
}

void UHTexture2DNode::OutputData(std::ofstream& FileOut)
{
	UHUtilities::WriteStringData(FileOut, SelectedTexturePathName);
}

void UHTexture2DNode::SetSelectedTexturePathName(std::string InSelectedTextureName)
{
	SelectedTexturePathName = InSelectedTextureName;
}

std::string UHTexture2DNode::GetSelectedTexturePathName() const
{
	return SelectedTexturePathName;
}

void UHTexture2DNode::SetTextureIndexInMaterial(int32_t InIndex)
{
	TextureIndexInMaterial = InIndex;
}