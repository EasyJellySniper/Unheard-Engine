#include "MaterialNode.h"
#include "ParameterNode.h"
#include "MathNode.h"
#include "TextureNode.h"
#include "../Material.h"
#include <unordered_map>
#include "../../Engine/Asset.h"

UHMaterialNode::UHMaterialNode(UHMaterial* InMat)
	: UHGraphNode(false)
	, MaterialCache(InMat)
{
	Name = "Material Inputs";

	// declare the input pins for material node
	Inputs.resize(UH_ENUM_VALUE(UHMaterialInputs::MaterialMax));
	Inputs[UH_ENUM_VALUE(UHMaterialInputs::Diffuse)] = MakeUnique<UHGraphPin>("Diffuse (RGB)", this, UHGraphPinType::Float3Pin);
	Inputs[UH_ENUM_VALUE(UHMaterialInputs::Occlusion)] = MakeUnique<UHGraphPin>("Occlusion (R)", this, UHGraphPinType::FloatPin);
	Inputs[UH_ENUM_VALUE(UHMaterialInputs::Specular)] = MakeUnique<UHGraphPin>("Specular (RGB)", this, UHGraphPinType::Float3Pin);
	Inputs[UH_ENUM_VALUE(UHMaterialInputs::Normal)] = MakeUnique<UHGraphPin>("Normal (RGB)", this, UHGraphPinType::Float3Pin);
	Inputs[UH_ENUM_VALUE(UHMaterialInputs::Opacity)] = MakeUnique<UHGraphPin>("Opacity (R)", this, UHGraphPinType::FloatPin);
	Inputs[UH_ENUM_VALUE(UHMaterialInputs::Metallic)] = MakeUnique<UHGraphPin>("Metallic (R)", this, UHGraphPinType::FloatPin);
	Inputs[UH_ENUM_VALUE(UHMaterialInputs::Roughness)] = MakeUnique<UHGraphPin>("Roughness (R)", this, UHGraphPinType::FloatPin);
	Inputs[UH_ENUM_VALUE(UHMaterialInputs::FresnelFactor)] = MakeUnique<UHGraphPin>("Fresnel Factor (R)", this, UHGraphPinType::FloatPin);
	Inputs[UH_ENUM_VALUE(UHMaterialInputs::Emissive)] = MakeUnique<UHGraphPin>("Emissive (RGB)", this, UHGraphPinType::Float3Pin);
	Inputs[UH_ENUM_VALUE(UHMaterialInputs::Refraction)] = MakeUnique<UHGraphPin>("Refraction (R)", this, UHGraphPinType::FloatPin);
}

void CollectTexDefinitions(const UHGraphPin* Pin, const bool bIsCompilingRayTracing, int32_t& TextureIndexInMaterial
	, std::vector<std::string>& OutDefinitions, std::unordered_map<uint32_t, bool>& OutDefTable)
{
	if (Pin->GetSrcPin() == nullptr || Pin->GetSrcPin()->GetOriginNode() == nullptr)
	{
		return;
	}

	UHGraphNode* InputNode = Pin->GetSrcPin()->GetOriginNode();
	InputNode->SetIsCompilingRayTracing(bIsCompilingRayTracing);

	// prevent redefinition with table
	if (OutDefTable.find(InputNode->GetId()) == OutDefTable.end() && InputNode->GetType() == UHGraphNodeType::Texture2DNode)
	{
		// set texture index in material so I can index in ray tracing shader properly
		UHTexture2DNode* TexNode = static_cast<UHTexture2DNode*>(InputNode);
		TexNode->SetTextureIndexInMaterial(TextureIndexInMaterial);
		TextureIndexInMaterial++;

		std::string Def = InputNode->EvalDefinition();
		if (!Def.empty())
		{
			OutDefinitions.push_back(Def);
		}

		OutDefTable[InputNode->GetId()] = true;
	}

	// trace all input pins
	for (const UniquePtr<UHGraphPin>& InputPins : InputNode->GetInputs())
	{
		CollectTexDefinitions(InputPins.get(), bIsCompilingRayTracing, TextureIndexInMaterial, OutDefinitions, OutDefTable);
	}
}

void CollectParameterDefinitions(const UHGraphPin* Pin, const bool bIsCompilingRayTracing, int32_t& DataIndexInMaterial
	, std::vector<std::string>& OutDefinitions, std::unordered_map<uint32_t, bool>& OutDefTable)
{
	if (Pin->GetSrcPin() == nullptr || Pin->GetSrcPin()->GetOriginNode() == nullptr)
	{
		return;
	}

	UHGraphNode* InputNode = Pin->GetSrcPin()->GetOriginNode();
	InputNode->SetIsCompilingRayTracing(bIsCompilingRayTracing);

	// prevent redefinition with table
	if (OutDefTable.find(InputNode->GetId()) == OutDefTable.end())
	{
		// set texture index in material so I can index in ray tracing shader properly
		bool bFound = false;
		switch (InputNode->GetType())
		{
		case UHGraphNodeType::FloatNode:
		{
			UHFloatNode* Node = static_cast<UHFloatNode*>(InputNode);
			Node->SetDataIndexInMaterial(DataIndexInMaterial);
			DataIndexInMaterial++;
			bFound = true;
			break;
		}
		case UHGraphNodeType::Float2Node:
		{
			UHFloat2Node* Node = static_cast<UHFloat2Node*>(InputNode);
			Node->SetDataIndexInMaterial(DataIndexInMaterial);
			DataIndexInMaterial += 2;
			bFound = true;
			break;
		}
		case UHGraphNodeType::Float3Node:
		{
			UHFloat3Node* Node = static_cast<UHFloat3Node*>(InputNode);
			Node->SetDataIndexInMaterial(DataIndexInMaterial);
			DataIndexInMaterial += 3;
			bFound = true;
			break;
		}
		case UHGraphNodeType::Float4Node:
		{
			UHFloat4Node* Node = static_cast<UHFloat4Node*>(InputNode);
			Node->SetDataIndexInMaterial(DataIndexInMaterial);
			DataIndexInMaterial += 4;
			bFound = true;
			break;
		}
		}

		if (bFound)
		{
			std::string Def = InputNode->EvalDefinition();
			if (!Def.empty())
			{
				OutDefinitions.push_back(Def);
			}

			OutDefTable[InputNode->GetId()] = true;
		}
	}

	// trace all input pins
	for (const UniquePtr<UHGraphPin>& InputPins : InputNode->GetInputs())
	{
		CollectParameterDefinitions(InputPins.get(), bIsCompilingRayTracing, DataIndexInMaterial, OutDefinitions, OutDefTable);
	}
}

void UHMaterialNode::InsertOpacityCode(std::string& Code) const
{
	std::string EndOfLine = ";\n";
	if (UHGraphPin* Opacity = Inputs[UH_ENUM_VALUE(UHMaterialInputs::Opacity)]->GetSrcPin())
	{
		Code += "\tInput.Opacity = saturate(" + Opacity->GetOriginNode()->EvalHLSL(Opacity) + ".r)" + EndOfLine;
	}
	else
	{
		Code += "\tInput.Opacity = 1.0f" + EndOfLine;
	}
}

void UHMaterialNode::InsertNormalCode(std::string& Code) const
{
	std::string EndOfLine = ";\n";
	if (UHGraphPin* Normal = Inputs[UH_ENUM_VALUE(UHMaterialInputs::Normal)]->GetSrcPin())
	{
		Code += "\tInput.Normal = " + Normal->GetOriginNode()->EvalHLSL(Normal) + ".rgb" + EndOfLine;
	}
	else
	{
		Code += "\tInput.Normal = float3(0,0,1.0f)" + EndOfLine;
	}
}

void UHMaterialNode::InsertRoughnessCode(std::string& Code) const
{
	std::string EndOfLine = ";\n";
	if (UHGraphPin* Roughness = Inputs[UH_ENUM_VALUE(UHMaterialInputs::Roughness)]->GetSrcPin())
	{
		Code += "\tInput.Roughness = saturate(" + Roughness->GetOriginNode()->EvalHLSL(Roughness) + ".r)" + EndOfLine;
	}
	else
	{
		Code += "\tInput.Roughness = 1.0f" + EndOfLine;
	}
}

void UHMaterialNode::InsertEmissiveCode(std::string& Code) const
{
	std::string EndOfLine = ";\n";
	if (UHGraphPin* Emissive = Inputs[UH_ENUM_VALUE(UHMaterialInputs::Emissive)]->GetSrcPin())
	{
		Code += "\tInput.Emissive = " + Emissive->GetOriginNode()->EvalHLSL(Emissive) + ".rgb" + EndOfLine;
	}
	else
	{
		Code += "\tInput.Emissive = float3(0,0,0)" + EndOfLine;
	}
}

std::string UHMaterialNode::EvalHLSL(const UHGraphPin* CallerPin)
{
	if (!CanEvalHLSL())
	{
		return "";
	}

	// parse definition
	std::vector<std::string> Definitions;
	std::unordered_map<uint32_t, bool> DefinitionTable;

	// set texture index data in material
	int32_t TextureIndexInMaterial = 0;
	for (int32_t Idx = 0; Idx < UH_ENUM_VALUE(UHMaterialInputs::MaterialMax); Idx++)
	{
		CollectTexDefinitions(Inputs[Idx].get(), CompileData.bIsHitGroup, TextureIndexInMaterial, Definitions, DefinitionTable);
	}

	// set data index in material as well, needs to follow the texture index data, for RT only
	if (CompileData.bIsHitGroup)
	{
		DefinitionTable.clear();

		// Calculate the DataIndexInMaterial, if it has no texture definitions, the starting index shall be GRTMaterialDataStartIndex
		// Otherwise, follow the texture index in material and properly offset it with GRTMaterialDataStartIndex
		// @TODO: Custom sampler index?
		int32_t DataIndexInMaterial = (Definitions.size() > 0) ? /*2 */ TextureIndexInMaterial + GRTMaterialDataStartIndex : GRTMaterialDataStartIndex;

		for (int32_t Idx = 0; Idx < UH_ENUM_VALUE(UHMaterialInputs::MaterialMax); Idx++)
		{
			CollectParameterDefinitions(Inputs[Idx].get(), CompileData.bIsHitGroup, DataIndexInMaterial, Definitions, DefinitionTable);
		}
	}

	// Calculate property based on graph nodes
	std::string EndOfLine = ";\n";
	std::string TabIdentifier = "\t";
	std::string ReturnCode = "\treturn Input;";
	std::string Code;

	for (int32_t Idx = static_cast<int32_t>(Definitions.size()) - 1; Idx >= 0; Idx--)
	{
		if (Idx == Definitions.size() - 1)
		{
			Code += Definitions[Idx];
		}
		else
		{
			Code += TabIdentifier + Definitions[Idx];
		}
	}

	Code += "\n\tUHMaterialInputs Input = (UHMaterialInputs)0;\n";

	// ************** Early return if it needs normal only ************** //
	if (CompileData.InputType == UHMaterialInputType::MaterialInputNormalOnly)
	{
		InsertNormalCode(Code);
		Code += ReturnCode;
		return Code;
	}

	// ************** Early return if it needs opacity only ************** //
	if (CompileData.InputType == UHMaterialInputType::MaterialInputOpacityOnly)
	{
		InsertOpacityCode(Code);
		Code += ReturnCode;
		return Code;
	}

	// ************** Early return if it needs smoothness only ************** //
	if (CompileData.InputType == UHMaterialInputType::MaterialInputSmoothnessOnly)
	{
		InsertRoughnessCode(Code);
		Code += ReturnCode;
		return Code;
	}

	// ************** Early return if it needs opacity+normal+roughness only ************** //
	if (CompileData.InputType == UHMaterialInputType::MaterialInputOpacityNormalRoughOnly)
	{
		InsertOpacityCode(Code);
		InsertNormalCode(Code);
		InsertRoughnessCode(Code);
		Code += ReturnCode;
		return Code;
	}

	// ************** Early return if it needs emissive only ************** //
	if (CompileData.InputType == UHMaterialInputType::MaterialInputEmissiveOnly)
	{
		InsertEmissiveCode(Code);
		Code += ReturnCode;
		return Code;
	}

	// Diffuse
	if (UHGraphPin* DiffuseSrc = Inputs[UH_ENUM_VALUE(UHMaterialInputs::Diffuse)]->GetSrcPin())
	{
		Code += "\tInput.Diffuse = " + DiffuseSrc->GetOriginNode()->EvalHLSL(DiffuseSrc) + ".rgb" + EndOfLine;
	}
	else
	{
		Code += "\tInput.Diffuse = 0.8f" + EndOfLine;
	}

	// Occlusion
	if (UHGraphPin* Occlusion = Inputs[UH_ENUM_VALUE(UHMaterialInputs::Occlusion)]->GetSrcPin())
	{
		Code += "\tInput.Occlusion = saturate(" + Occlusion->GetOriginNode()->EvalHLSL(Occlusion) + ".r)" + EndOfLine;
	}
	else
	{
		Code += "\tInput.Occlusion = 1.0f" + EndOfLine;
	}

	// Specular
	if (UHGraphPin* Specular = Inputs[UH_ENUM_VALUE(UHMaterialInputs::Specular)]->GetSrcPin())
	{
		Code += "\tInput.Specular = saturate(" + Specular->GetOriginNode()->EvalHLSL(Specular) + ".rgb)" + EndOfLine;
	}
	else
	{
		Code += "\tInput.Specular = 0.5f" + EndOfLine;
	}

	// Roughness
	InsertRoughnessCode(Code);

	// Normal
	InsertNormalCode(Code);

	// Opacity
	InsertOpacityCode(Code);

	// Metallic
	if (UHGraphPin* Metallic = Inputs[UH_ENUM_VALUE(UHMaterialInputs::Metallic)]->GetSrcPin())
	{
		Code += "\tInput.Metallic = saturate(" + Metallic->GetOriginNode()->EvalHLSL(Metallic) + ".r)" + EndOfLine;
	}
	else
	{
		Code += "\tInput.Metallic = 0.0f" + EndOfLine;
	}

	// FresnelFactor
	if (UHGraphPin* FresnelFactor = Inputs[UH_ENUM_VALUE(UHMaterialInputs::FresnelFactor)]->GetSrcPin())
	{
		Code += "\tInput.FresnelFactor = saturate(" + FresnelFactor->GetOriginNode()->EvalHLSL(FresnelFactor) + ".r)" + EndOfLine;
	}
	else
	{
		Code += "\tInput.FresnelFactor = 0.0f" + EndOfLine;
	}

	InsertEmissiveCode(Code);

	// Refraction, available for translucent objects only
	if (UHGraphPin* Refraction = Inputs[UH_ENUM_VALUE(UHMaterialInputs::Refraction)]->GetSrcPin())
	{
		if (CompileData.MaterialCache->GetBlendMode() > UHBlendMode::Masked)
		{
			Code += "\tInput.Refraction = max(" + Refraction->GetOriginNode()->EvalHLSL(Refraction) + ".r, 0.01f)" + EndOfLine;
		}
	}

	Code += ReturnCode;

	return Code;
}

UniquePtr<UHGraphNode> AllocateNewGraphNode(UHGraphNodeType InType)
{
	UniquePtr<UHGraphNode> NewNode;

	switch (InType)
	{
	case UHGraphNodeType::FloatNode:
		NewNode = MakeUnique<UHFloatNode>();
		break;
	case UHGraphNodeType::Float2Node:
		NewNode = MakeUnique<UHFloat2Node>();
		break;
	case UHGraphNodeType::Float3Node:
		NewNode = MakeUnique<UHFloat3Node>();
		break;
	case UHGraphNodeType::Float4Node:
		NewNode = MakeUnique<UHFloat4Node>();
		break;
	case UHGraphNodeType::MathNode:
		NewNode = MakeUnique<UHMathNode>();
		break;
	case UHGraphNodeType::Texture2DNode:
		NewNode = MakeUnique<UHTexture2DNode>();
		break;
	}

	return std::move(NewNode);
}

void UHMaterialNode::SetMaterialCompileData(UHMaterialCompileData InData)
{
	CompileData = InData;
}

void CollectTextureIndexInternal(const UHGraphPin* Pin, std::string& Code, std::unordered_map<uint32_t, bool>& OutDefTable, size_t& OutSize)
{
	if (Pin->GetSrcPin() == nullptr || Pin->GetSrcPin()->GetOriginNode() == nullptr)
	{
		return;
	}

	UHGraphNode* InputNode = Pin->GetSrcPin()->GetOriginNode();

	// prevent redefinition with table
	if (OutDefTable.find(InputNode->GetId()) == OutDefTable.end() && InputNode->GetType() == UHGraphNodeType::Texture2DNode)
	{
		Code += "\tint Node_" + std::to_string(InputNode->GetId()) + "_Index;\n";
		OutSize += sizeof(float);
		OutDefTable[InputNode->GetId()] = true;
	}

	// trace all input pins
	for (const UniquePtr<UHGraphPin>& InputPins : InputNode->GetInputs())
	{
		CollectTextureIndexInternal(InputPins.get(), Code, OutDefTable, OutSize);
	}
}

void UHMaterialNode::CollectTextureIndex(std::string& Code, size_t& OutSize)
{
	std::unordered_map<uint32_t, bool> TexTable;

	// simply collect the texture index used in bindless rendering
	for (const UniquePtr<UHGraphPin>& Input : GetInputs())
	{
		CollectTextureIndexInternal(Input.get(), Code, TexTable, OutSize);
	}
}

void CollectTextureNameInternal(const UHGraphPin* Pin, std::vector<std::string>& Names, std::unordered_map<uint32_t, bool>& OutDefTable)
{
	if (Pin->GetSrcPin() == nullptr || Pin->GetSrcPin()->GetOriginNode() == nullptr)
	{
		return;
	}

	UHGraphNode* InputNode = Pin->GetSrcPin()->GetOriginNode();

	// prevent redefinition with table
	if (OutDefTable.find(InputNode->GetId()) == OutDefTable.end() && InputNode->GetType() == UHGraphNodeType::Texture2DNode)
	{
		UHTexture2DNode* TexNode = static_cast<UHTexture2DNode*>(InputNode);
		std::string TexName = TexNode->GetSelectedTexturePathName();
#if WITH_EDITOR
		TexName = UHAssetManager::FindTexturePathName(TexName);
#endif
		if (!TexName.empty())
		{
			Names.push_back(TexName);
		}

		OutDefTable[InputNode->GetId()] = true;
	}

	// trace all input pins
	for (const UniquePtr<UHGraphPin>& InputPins : InputNode->GetInputs())
	{
		CollectTextureNameInternal(InputPins.get(), Names, OutDefTable);
	}
}

void UHMaterialNode::CollectTextureNames(std::vector<std::string>& Names)
{
	Names.clear();
	std::unordered_map<uint32_t, bool> TexTable;

	for (const UniquePtr<UHGraphPin>& Input : GetInputs())
	{
		CollectTextureNameInternal(Input.get(), Names, TexTable);
	}
}

void ParameterPadding(std::string& Code, size_t& OutSize, int32_t& PaddingNo, const size_t Stride)
{
	// this function detect before adding a parameter define, see if it's padding to 16 bytes
	// that's the cbuffer rule
	const size_t CurrOffset = OutSize % 16;
	const size_t AfterSize = CurrOffset + Stride;
	if (AfterSize > 16)
	{
		// need to padding
		for (size_t Idx = 0; Idx < (16 - CurrOffset) / 4; Idx++)
		{
			Code += "float Padding" + std::to_string(PaddingNo) + ";\n";
			PaddingNo++;
		}
		OutSize += 16 - CurrOffset;
	}
}

void CopyAddressPadding(size_t& OutAddress, const size_t Stride)
{
	// this function detect before copying a parameter, see if it's padding to 16 bytes
	// that's the cbuffer rule
	const size_t CurrOffset = OutAddress % 16;
	const size_t AfterSize = CurrOffset + Stride;
	if (AfterSize > 16)
	{
		// adjust the copy address, need to skip the padding value themselves too
		OutAddress += (16 - CurrOffset);
	}
}

void CollectParameterInternal(const UHGraphPin* Pin, std::string& Code, std::unordered_map<uint32_t, bool>& OutDefTable, size_t& OutSize
	, int32_t& PaddingNo)
{
	if (Pin->GetSrcPin() == nullptr || Pin->GetSrcPin()->GetOriginNode() == nullptr)
	{
		return;
	}

	UHGraphNode* InputNode = Pin->GetSrcPin()->GetOriginNode();

	// prevent redefinition with table
	if (OutDefTable.find(InputNode->GetId()) == OutDefTable.end())
	{
		// add parameter code based on type
		switch (InputNode->GetType())
		{
		case UHGraphNodeType::FloatNode:
		{
			ParameterPadding(Code, OutSize, PaddingNo, sizeof(float));
			Code += "\tfloat Node_" + std::to_string(InputNode->GetId()) + ";\n";
			OutSize += sizeof(float);
			break;
		}
		case UHGraphNodeType::Float2Node:
		{
			ParameterPadding(Code, OutSize, PaddingNo, sizeof(float) * 2);
			Code += "\tfloat2 Node_" + std::to_string(InputNode->GetId()) + ";\n";
			OutSize += sizeof(float) * 2;
			break;
		}
		case UHGraphNodeType::Float3Node:
		{
			ParameterPadding(Code, OutSize, PaddingNo, sizeof(float) * 3);
			Code += "\tfloat3 Node_" + std::to_string(InputNode->GetId()) + ";\n";
			OutSize += sizeof(float) * 3;
			break;
		}
		case UHGraphNodeType::Float4Node:
		{
			ParameterPadding(Code, OutSize, PaddingNo, sizeof(float) * 4);
			Code += "\tfloat4 Node_" + std::to_string(InputNode->GetId()) + ";\n";
			OutSize += sizeof(float) * 4;
			break;
		}
		}

		OutDefTable[InputNode->GetId()] = true;
	}

	// trace all input pins
	for (const UniquePtr<UHGraphPin>& InputPins : InputNode->GetInputs())
	{
		CollectParameterInternal(InputPins.get(), Code, OutDefTable, OutSize, PaddingNo);
	}
}

void UHMaterialNode::CollectMaterialParameter(std::string& Code, size_t& OutSize)
{
	std::unordered_map<uint32_t, bool> ParamTable;
	int32_t PaddingNo = 0;

	for (const UniquePtr<UHGraphPin>& Input : GetInputs())
	{
		CollectParameterInternal(Input.get(), Code, ParamTable, OutSize, PaddingNo);
	}
}

void CopyParameterInternal(const UHGraphPin* Pin, std::vector<uint8_t>& MaterialData, std::unordered_map<uint32_t, bool>& OutDefTable, size_t& BufferAddress)
{
	if (Pin->GetSrcPin() == nullptr || Pin->GetSrcPin()->GetOriginNode() == nullptr)
	{
		return;
	}

	UHGraphNode* InputNode = Pin->GetSrcPin()->GetOriginNode();

	// prevent redefinition with table
	if (OutDefTable.find(InputNode->GetId()) == OutDefTable.end())
	{
		// add parameter code based on type
		switch (InputNode->GetType())
		{
		case UHGraphNodeType::FloatNode:
		{
			CopyAddressPadding(BufferAddress, sizeof(float));
			float Val = ((UHFloatNode*)InputNode)->GetValue();
			memcpy_s(MaterialData.data() + BufferAddress, sizeof(float), &Val, sizeof(float));
			BufferAddress += sizeof(float);
			break;
		}
		case UHGraphNodeType::Float2Node:
		{
			CopyAddressPadding(BufferAddress, sizeof(float) * 2);
			XMFLOAT2 Val = ((UHFloat2Node*)InputNode)->GetValue();
			memcpy_s(MaterialData.data() + BufferAddress, sizeof(float) * 2, &Val, sizeof(float) * 2);
			BufferAddress += sizeof(float) * 2;
			break;
		}
		case UHGraphNodeType::Float3Node:
		{
			CopyAddressPadding(BufferAddress, sizeof(float) * 3);
			XMFLOAT3 Val = ((UHFloat3Node*)InputNode)->GetValue();
			memcpy_s(MaterialData.data() + BufferAddress, sizeof(float) * 3, &Val, sizeof(float) * 3);
			BufferAddress += sizeof(float) * 3;
			break;
		}
		case UHGraphNodeType::Float4Node:
		{
			CopyAddressPadding(BufferAddress, sizeof(float) * 4);
			XMFLOAT4 Val = ((UHFloat4Node*)InputNode)->GetValue();
			memcpy_s(MaterialData.data() + BufferAddress, sizeof(float) * 4, &Val, sizeof(float) * 4);
			BufferAddress += sizeof(float) * 4;
			break;
		}
		}

		OutDefTable[InputNode->GetId()] = true;
	}

	// trace all input pins
	for (const UniquePtr<UHGraphPin>& InputPins : InputNode->GetInputs())
	{
		CopyParameterInternal(InputPins.get(), MaterialData, OutDefTable, BufferAddress);
	}
}

void UHMaterialNode::CopyMaterialParameter(std::vector<uint8_t>& MaterialData, size_t& BufferAddress)
{
	std::unordered_map<uint32_t, bool> ParamTable;
	for (const UniquePtr<UHGraphPin>& Input : GetInputs())
	{
		CopyParameterInternal(Input.get(), MaterialData, ParamTable, BufferAddress);
	}
}

void CopyRTParameterInternal(const UHGraphPin* Pin, UHRTMaterialData& MaterialData, std::unordered_map<uint32_t, bool>& OutDefTable, int32_t& DstIndex)
{
	if (Pin->GetSrcPin() == nullptr || Pin->GetSrcPin()->GetOriginNode() == nullptr)
	{
		return;
	}

	UHGraphNode* InputNode = Pin->GetSrcPin()->GetOriginNode();

	// prevent redefinition with table
	if (OutDefTable.find(InputNode->GetId()) == OutDefTable.end())
	{
		// add parameter code based on type
		switch (InputNode->GetType())
		{
		case UHGraphNodeType::FloatNode:
		{
			float Val = ((UHFloatNode*)InputNode)->GetValue();
			memcpy_s(&MaterialData.Data[DstIndex], sizeof(float), &Val, sizeof(float));
			DstIndex++;
			break;
		}
		case UHGraphNodeType::Float2Node:
		{
			XMFLOAT2 Val = ((UHFloat2Node*)InputNode)->GetValue();
			memcpy_s(&MaterialData.Data[DstIndex], sizeof(float) * 2, &Val, sizeof(float) * 2);
			DstIndex += 2;
			break;
		}
		case UHGraphNodeType::Float3Node:
		{
			XMFLOAT3 Val = ((UHFloat3Node*)InputNode)->GetValue();
			memcpy_s(&MaterialData.Data[DstIndex], sizeof(float) * 3, &Val, sizeof(float) * 3);
			DstIndex += 3;
			break;
		}
		case UHGraphNodeType::Float4Node:
		{
			XMFLOAT4 Val = ((UHFloat4Node*)InputNode)->GetValue();
			memcpy_s(&MaterialData.Data[DstIndex], sizeof(float) * 4, &Val, sizeof(float) * 4);
			DstIndex += 4;
			break;
		}
		}

		OutDefTable[InputNode->GetId()] = true;
	}

	// trace all input pins
	for (const UniquePtr<UHGraphPin>& InputPins : InputNode->GetInputs())
	{
		CopyRTParameterInternal(InputPins.get(), MaterialData, OutDefTable, DstIndex);
	}
}

void UHMaterialNode::CopyRTMaterialParameter(UHRTMaterialData& RTMaterialData, int32_t& DstIndex)
{
	std::unordered_map<uint32_t, bool> ParamTable;
	for (const UniquePtr<UHGraphPin>& Input : GetInputs())
	{
		CopyRTParameterInternal(Input.get(), RTMaterialData, ParamTable, DstIndex);
	}
}