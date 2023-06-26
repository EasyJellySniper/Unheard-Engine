#include "MaterialNode.h"
#include "ParameterNode.h"
#include "MathNode.h"
#include "TextureNode.h"
#include "../Material.h"
#include <unordered_map>

UHMaterialNode::UHMaterialNode(UHMaterial* InMat)
	: UHGraphNode(false)
	, MaterialCache(InMat)
{
	Name = "Material Inputs";

	// declare the input pins for material node
	Inputs.resize(UHMaterialInputs::MaterialMax);
	Inputs[UHMaterialInputs::Diffuse] = std::make_unique<UHGraphPin>("Diffuse (RGB)", this, Float3Node);
	Inputs[UHMaterialInputs::Occlusion] = std::make_unique<UHGraphPin>("Occlusion (R)", this, FloatNode);
	Inputs[UHMaterialInputs::Specular] = std::make_unique<UHGraphPin>("Specular (RGB)", this, Float3Node);
	Inputs[UHMaterialInputs::Normal] = std::make_unique<UHGraphPin>("Normal (RGB)", this, Float3Node);
	Inputs[UHMaterialInputs::Opacity] = std::make_unique<UHGraphPin>("Opacity (R)", this, FloatNode);
	Inputs[UHMaterialInputs::Metallic] = std::make_unique<UHGraphPin>("Metallic (R)", this, FloatNode);
	Inputs[UHMaterialInputs::Roughness] = std::make_unique<UHGraphPin>("Roughness (R)", this, FloatNode);
	Inputs[UHMaterialInputs::FresnelFactor] = std::make_unique<UHGraphPin>("Fresnel Factor (R)", this, FloatNode);
	Inputs[UHMaterialInputs::ReflectionFactor] = std::make_unique<UHGraphPin>("Reflection Factor (R)", this, FloatNode);
	Inputs[UHMaterialInputs::Emissive] = std::make_unique<UHGraphPin>("Emissive (RGB)", this, Float3Node);
}

void CollectDefinitions(const UHGraphPin* Pin, const bool bIsCompilingRayTracing, int32_t& TextureIndexInMaterial
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
		if (InputNode->GetType() == Texture2DNode)
		{
			UHTexture2DNode* TexNode = static_cast<UHTexture2DNode*>(InputNode);
			TexNode->SetTextureIndexInMaterial(TextureIndexInMaterial);
			TextureIndexInMaterial++;
		}

		std::string Def = InputNode->EvalDefinition();
		if (!Def.empty())
		{
			OutDefinitions.push_back(Def);
		}

		OutDefTable[InputNode->GetId()] = true;
	}

	// trace all input pins
	for (const std::unique_ptr<UHGraphPin>& InputPins : InputNode->GetInputs())
	{
		CollectDefinitions(InputPins.get(), bIsCompilingRayTracing, TextureIndexInMaterial, OutDefinitions, OutDefTable);
	}
}

std::string UHMaterialNode::EvalHLSL()
{
	if (!CanEvalHLSL())
	{
		return "";
	}

	// parse definition
	std::vector<std::string> Definitions;
	std::unordered_map<uint32_t, bool> DefinitionTable;

	// set texture index in material
	int32_t TextureIndexInMaterial = 0;

	for (int32_t Idx = 0; Idx < UHMaterialInputs::MaterialMax; Idx++)
	{
		CollectDefinitions(Inputs[Idx].get(), CompileData.bIsHitGroup, TextureIndexInMaterial, Definitions, DefinitionTable);
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

	// Opacity
	if (UHGraphPin* Opacity = Inputs[UHMaterialInputs::Opacity]->GetSrcPin())
	{
		Code += "\tInput.Opacity = " + Opacity->GetOriginNode()->EvalHLSL() + ".r" + EndOfLine;
	}
	else
	{
		Code += "\tInput.Opacity = 1.0f" + EndOfLine;
	}

	// ************** Early return if this is for depth or motion pass only ************** //
	if (CompileData.bIsDepthOrMotionPass)
	{
		Code += ReturnCode;
		return Code;
	}

	// Diffuse
	if (UHGraphPin* DiffuseSrc = Inputs[UHMaterialInputs::Diffuse]->GetSrcPin())
	{
		Code += "\tInput.Diffuse = " + DiffuseSrc->GetOriginNode()->EvalHLSL() + ".rgb" + EndOfLine;
	}
	else
	{
		Code += "\tInput.Diffuse = 0.8f" + EndOfLine;
	}

	// Occlusion
	if (UHGraphPin* Occlusion = Inputs[UHMaterialInputs::Occlusion]->GetSrcPin())
	{
		Code += "\tInput.Occlusion = " + Occlusion->GetOriginNode()->EvalHLSL() + ".r" + EndOfLine;
	}
	else
	{
		Code += "\tInput.Occlusion = 1.0f" + EndOfLine;
	}

	// Specular
	if (UHGraphPin* Specular = Inputs[UHMaterialInputs::Specular]->GetSrcPin())
	{
		Code += "\tInput.Specular = " + Specular->GetOriginNode()->EvalHLSL() + ".rgb" + EndOfLine;
	}
	else
	{
		Code += "\tInput.Specular = 0.5f" + EndOfLine;
	}

	// Normal
	if (UHGraphPin* Normal = Inputs[UHMaterialInputs::Normal]->GetSrcPin())
	{
		Code += "\tInput.Normal = " + Normal->GetOriginNode()->EvalHLSL() + ".rgb" + EndOfLine;
	}
	else
	{
		Code += "\tInput.Normal = float3(0,0,1.0f)" + EndOfLine;
	}

	// Metallic
	if (UHGraphPin* Metallic = Inputs[UHMaterialInputs::Metallic]->GetSrcPin())
	{
		Code += "\tInput.Metallic = " + Metallic->GetOriginNode()->EvalHLSL() + ".r" + EndOfLine;
	}
	else
	{
		Code += "\tInput.Metallic = 0.0f" + EndOfLine;
	}

	// Roughness
	if (UHGraphPin* Roughness = Inputs[UHMaterialInputs::Roughness]->GetSrcPin())
	{
		Code += "\tInput.Roughness = " + Roughness->GetOriginNode()->EvalHLSL() + ".r" + EndOfLine;
	}
	else
	{
		Code += "\tInput.Roughness = 1.0f" + EndOfLine;
	}

	// FresnelFactor
	if (UHGraphPin* FresnelFactor = Inputs[UHMaterialInputs::FresnelFactor]->GetSrcPin())
	{
		Code += "\tInput.FresnelFactor = " + FresnelFactor->GetOriginNode()->EvalHLSL() + ".r" + EndOfLine;
	}
	else
	{
		Code += "\tInput.FresnelFactor = 1.0f" + EndOfLine;
	}

	// ReflectionFactor
	if (UHGraphPin* ReflectionFactor = Inputs[UHMaterialInputs::ReflectionFactor]->GetSrcPin())
	{
		Code += "\tInput.ReflectionFactor = " + ReflectionFactor->GetOriginNode()->EvalHLSL() + ".r" + EndOfLine;
	}
	else
	{
		Code += "\tInput.ReflectionFactor = 0.5f" + EndOfLine;
	}

	// Emissive
	if (UHGraphPin* Emissive = Inputs[UHMaterialInputs::Emissive]->GetSrcPin())
	{
		Code += "\tInput.Emissive = " + Emissive->GetOriginNode()->EvalHLSL() + ".rgb" + EndOfLine;
	}
	else
	{
		Code += "\tInput.Emissive = float3(0,0,0)" + EndOfLine;
	}

	Code += ReturnCode;

	return Code;
}

std::unique_ptr<UHGraphNode> AllocateNewGraphNode(UHGraphNodeType InType)
{
	std::unique_ptr<UHGraphNode> NewNode;

	switch (InType)
	{
	case UHGraphNodeType::Float:
		NewNode = std::make_unique<UHFloatNode>();
		break;
	case UHGraphNodeType::Float2:
		NewNode = std::make_unique<UHFloat2Node>();
		break;
	case UHGraphNodeType::Float3:
		NewNode = std::make_unique<UHFloat3Node>();
		break;
	case UHGraphNodeType::Float4:
		NewNode = std::make_unique<UHFloat4Node>();
		break;
	case UHGraphNodeType::MathNode:
		NewNode = std::make_unique<UHMathNode>();
		break;
	case UHGraphNodeType::Texture2DNode:
		NewNode = std::make_unique<UHTexture2DNode>();
		break;
	}

	return std::move(NewNode);
}

void UHMaterialNode::SetMaterialCompileData(UHMaterialCompileData InData)
{
	CompileData = InData;
}