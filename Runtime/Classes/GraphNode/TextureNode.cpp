#include "TextureNode.h"
#include "../Utility.h"
#include "../MaterialLayout.h"

static std::vector<std::string> BumpTextures;

UHTexture2DNode::UHTexture2DNode(std::string TexName)
	: UHGraphNode(true)
{
	Name = "Texture2D";
	NodeType = Texture2DNode;
	TextureIndexInMaterial = -1;

	Inputs.resize(1);
	Inputs[0] = std::make_unique<UHGraphPin>("UV", this, Float2Node);

	Outputs.resize(1);
	Outputs[0] = std::make_unique<UHGraphPin>("RGB", this, Float3Node);

	SelectedTextureName = TexName;

	// @TODO: Integrated this to texture editor if I have one, temporary stupid method
	if (BumpTextures.size() == 0)
	{
		std::ifstream FileIn("BumpTextures.txt", std::ios::in);
		if (FileIn.is_open())
		{
			while (!FileIn.eof())
			{
				std::string BumpTex;
				std::getline(FileIn, BumpTex);
				BumpTextures.push_back(BumpTex);
			}
		}
		FileIn.close();
	}
}

bool UHTexture2DNode::CanEvalHLSL()
{
	if (SelectedTextureName.empty())
	{
		return false;
	}

	return true;
}

std::string UHTexture2DNode::EvalDefinition()
{
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
		// @TODO: Remove this temporary method
		const bool bIsBumpTexture = UHUtilities::FindByElement(BumpTextures, SelectedTextureName);
		const std::string BumpDecode = bIsBumpTexture ? " * 2.0f - 1.0f" : "";

		// if it's compiling for ray tracing, I need to use the SampleLevel instead of Sample
		if (bIsCompilingRayTracing)
		{
			const std::string TextureIndexCode = "UHMaterialDataTable[InstanceID()][" + std::to_string(TextureIndexInMaterial) + "].TextureIndex";
			const std::string SamplerIndexCode = "UHMaterialDataTable[InstanceID()][" + std::to_string(TextureIndexInMaterial) + "].SamplerIndex";

			// the mip level will be calculated in the shader
			return "float4 Result_" + IDString + " = UHTextureTable[" + TextureIndexCode + "].SampleLevel(UHSamplerTable[" + SamplerIndexCode + "], " + UVString + ", MipLevel)"
				+ BumpDecode + ";\n";
		}

		const std::string TextureIndexCode = "Node_" + IDString + "_Index";
		const std::string SamplerIndexCode = GDefaultSamplerName + "_Index";
		return "float4 Result_" + IDString + " = UHTextureTable[" + TextureIndexCode + "].Sample(UHSamplerTable[" + SamplerIndexCode + "], " + UVString + ")" + BumpDecode + ";\n";
	}

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
	UHUtilities::ReadStringData(FileIn, SelectedTextureName);
}

void UHTexture2DNode::OutputData(std::ofstream& FileOut)
{
	UHUtilities::WriteStringData(FileOut, SelectedTextureName);
}

void UHTexture2DNode::SetSelectedTextureName(std::string InSelectedTextureName)
{
	SelectedTextureName = InSelectedTextureName;
}

std::string UHTexture2DNode::GetSelectedTextureName() const
{
	return SelectedTextureName;
}

void UHTexture2DNode::SetTextureIndexInMaterial(int32_t InIndex)
{
	TextureIndexInMaterial = InIndex;
}