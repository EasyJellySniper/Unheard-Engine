#include "TextureNode.h"
#include "../Utility.h"
#include "../MaterialLayout.h"
#include "../../Engine/Asset.h"

UHTexture2DNode::UHTexture2DNode(std::string TexName)
	: UHGraphNode(true)
{
	Name = "Texture2D";
	NodeType = Texture2DNode;
	TextureIndexInMaterial = -1;

	Inputs.resize(1);
	Inputs[0] = MakeUnique<UHGraphPin>("UV", this, Float2Node);

	Outputs.resize(1);
	Outputs[0] = MakeUnique<UHGraphPin>("RGB", this, Float3Node);

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
#if WITH_DEBUG
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
			UVString = Inputs[0]->GetSrcPin()->GetOriginNode()->EvalHLSL();
		}

		// consider if this is a bump texture and insert decode
		const bool bIsBumpTexture = UHAssetManager::IsBumpTexture(SelectedTexturePathName);

		// if it's compiling for ray tracing, I need to use the SampleLevel instead of Sample
		if (bIsCompilingRayTracing)
		{
			const std::string TextureIndexCode = "UHMaterialDataTable[InstanceID()][" + std::to_string(TextureIndexInMaterial) + "].TextureIndex";
			const std::string SamplerIndexCode = "UHMaterialDataTable[InstanceID()][" + std::to_string(TextureIndexInMaterial) + "].SamplerIndex";

			// the mip level will be calculated in the shader
			std::string Result =
				"float4 Result_" + IDString + " = UHTextureTable[" + TextureIndexCode + "].SampleLevel(UHSamplerTable[" + SamplerIndexCode + "], " + UVString + ", MipLevel);\n";

			if (bIsBumpTexture)
			{
				// insert decode bump normal code if it's normal map
				Result += "Result_" + IDString + ".xyz = DecodeNormal(" + "Result_" + IDString + ".xyz" + ");\n";
			}

			return Result;
		}

		const std::string TextureIndexCode = "Node_" + IDString + "_Index";
		const std::string SamplerIndexCode = GDefaultSamplerName + "_Index";
		std::string Result = "float4 Result_" + IDString + " = UHTextureTable[" + TextureIndexCode + "].Sample(UHSamplerTable[" + SamplerIndexCode + "], " + UVString + ");\n";

		if (bIsBumpTexture)
		{
			// insert decode bump normal code if it's normal map
			Result += "Result_" + IDString + ".xyz = DecodeNormal(" + "Result_" + IDString + ".xyz" + ");\n";
		}
		return Result;
	}
#endif
	return "[ERROR] Texture not set.";
}

std::string UHTexture2DNode::EvalHLSL()
{
	if (CanEvalHLSL())
	{
		// @TODO: Consider channel variants
		const std::string IDString = std::to_string(GetId());
		return "Result_" + IDString + ".rgb";
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