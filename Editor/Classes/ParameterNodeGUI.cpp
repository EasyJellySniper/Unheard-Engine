#include "ParameterNodeGUI.h"

#if WITH_EDITOR
#include "EditorUtils.h"
#include "Runtime/Classes/Utility.h"
#include "Runtime/Renderer/DeferredShadingRenderer.h"

UHFloatNodeGUI::UHFloatNodeGUI(UHDeferredShadingRenderer* InRenderer, UHMaterial* InMat)
	: Node(nullptr)
	, Renderer(InRenderer)
	, MaterialCache(InMat)
{

}

UHFloat2NodeGUI::UHFloat2NodeGUI(UHDeferredShadingRenderer* InRenderer, UHMaterial* InMat)
	: Node(nullptr)
	, Renderer(InRenderer)
	, MaterialCache(InMat)
{

}

UHFloat3NodeGUI::UHFloat3NodeGUI(UHDeferredShadingRenderer* InRenderer, UHMaterial* InMat)
	: Node(nullptr)
	, Renderer(InRenderer)
	, MaterialCache(InMat)
{

}

UHFloat4NodeGUI::UHFloat4NodeGUI(UHDeferredShadingRenderer* InRenderer, UHMaterial* InMat)
	: Node(nullptr)
	, Renderer(InRenderer)
	, MaterialCache(InMat)
{

}

void UHFloatNodeGUI::SetDefaultValueFromGUI()
{
	Node->SetValue(InputsTextFields[0]->Parse<float>());
}

void UHFloat2NodeGUI::SetDefaultValueFromGUI()
{
	UHVector2 Value;
	Value.X = InputsTextFields[0]->Parse<float>();
	Value.Y = InputsTextFields[1]->Parse<float>();
	Node->SetValue(Value);
}

void UHFloat3NodeGUI::SetDefaultValueFromGUI()
{
	UHVector3 Value;
	Value.X = InputsTextFields[0]->Parse<float>();
	Value.Y = InputsTextFields[1]->Parse<float>();
	Value.Z = InputsTextFields[2]->Parse<float>();
	Node->SetValue(Value);
}

void UHFloat4NodeGUI::SetDefaultValueFromGUI()
{
	UHVector4 Value;
	Value.X = InputsTextFields[0]->Parse<float>();
	Value.Y = InputsTextFields[1]->Parse<float>();
	Value.Z = InputsTextFields[2]->Parse<float>();
	Value.W = InputsTextFields[3]->Parse<float>();
	Node->SetValue(Value);
}

void UHFloatNodeGUI::Update()
{
	// see if the value is changed and update the material shaders
	// the edit control callback doesn't work when it's a child, so using the update method
	float Val = Node->GetValue();
	SetDefaultValueFromGUI();

	if (Val == Node->GetValue())
	{
		return;
	}

	if (MaterialCache->IsMaterialNodeDirty())
	{
		return;
	}

	// mark compile flag as bind only if this is connecting to others
	if (Node->GetOutputs()[0]->GetDestPins().size() > 0)
	{
		MaterialCache->SetCompileFlag(UHMaterialCompileFlag::BindOnly);
		MaterialCache->SetRenderDirties(true);
		Renderer->RefreshMaterialShaders(MaterialCache, false, false);
	}
}

void UHFloat2NodeGUI::Update()
{
	UHVector2 Val = Node->GetValue();
	SetDefaultValueFromGUI();

	if (UHMathHelpers::IsVectorEqual(Val, Node->GetValue()))
	{
		return;
	}

	if (MaterialCache->IsMaterialNodeDirty())
	{
		return;
	}

	// mark compile flag as bind only if this is connecting to others
	if (Node->GetOutputs()[0]->GetDestPins().size() > 0)
	{
		MaterialCache->SetCompileFlag(UHMaterialCompileFlag::BindOnly);
		MaterialCache->SetRenderDirties(true);
		Renderer->RefreshMaterialShaders(MaterialCache, false, false);
	}
}

void UHFloat3NodeGUI::Update()
{
	UHVector3 Val = Node->GetValue();
	SetDefaultValueFromGUI();

	if (UHMathHelpers::IsVectorEqual(Val, Node->GetValue()))
	{
		return;
	}

	if (MaterialCache->IsMaterialNodeDirty())
	{
		return;
	}

	// mark compile flag as bind only if this is connecting to others
	if (Node->GetOutputs()[0]->GetDestPins().size() > 0)
	{
		MaterialCache->SetCompileFlag(UHMaterialCompileFlag::BindOnly);
		MaterialCache->SetRenderDirties(true);
		Renderer->RefreshMaterialShaders(MaterialCache, false, false);
	}
}

void UHFloat4NodeGUI::Update()
{
	UHVector4 Val = Node->GetValue();
	SetDefaultValueFromGUI();

	if (UHMathHelpers::IsVectorEqual(Val, Node->GetValue()))
	{
		return;
	}

	if (MaterialCache->IsMaterialNodeDirty())
	{
		return;
	}

	// mark compile flag as bind only if this is connecting to others
	if (Node->GetOutputs()[0]->GetDestPins().size() > 0)
	{
		MaterialCache->SetCompileFlag(UHMaterialCompileFlag::BindOnly);
		MaterialCache->SetRenderDirties(true);
		Renderer->RefreshMaterialShaders(MaterialCache, false, false);
	}
}

void UHFloatNodeGUI::PreAddingPins()
{
	Node = static_cast<UHFloatNode*>(GraphNode);
	InputTextFieldLength = 35;
}

void UHFloat2NodeGUI::PreAddingPins()
{
	Node = static_cast<UHFloat2Node*>(GraphNode);
	InputTextFieldLength = 35;
}

void UHFloat3NodeGUI::PreAddingPins()
{
	Node = static_cast<UHFloat3Node*>(GraphNode);
	InputTextFieldLength = 35;
}

void UHFloat4NodeGUI::PreAddingPins()
{
	Node = static_cast<UHFloat4Node*>(GraphNode);
	InputTextFieldLength = 35;
}

void UHFloatNodeGUI::PostAddingPins()
{
	// sync value to control
	InputsTextFields[0]->SetText(UHUtilities::FloatToWString(Node->GetValue()));
}

void UHFloat2NodeGUI::PostAddingPins()
{
	// sync value to control
	UHVector2 Value = Node->GetValue();
	InputsTextFields[0]->SetText(UHUtilities::FloatToWString(Value.X));
	InputsTextFields[1]->SetText(UHUtilities::FloatToWString(Value.Y));
}

void UHFloat3NodeGUI::PostAddingPins()
{
	// sync value to control
	UHVector3 Value = Node->GetValue();
	InputsTextFields[0]->SetText(UHUtilities::FloatToWString(Value.X));
	InputsTextFields[1]->SetText(UHUtilities::FloatToWString(Value.Y));
	InputsTextFields[2]->SetText(UHUtilities::FloatToWString(Value.Z));
}

void UHFloat4NodeGUI::PostAddingPins()
{
	// sync value to control
	UHVector4 Value = Node->GetValue();
	InputsTextFields[0]->SetText(UHUtilities::FloatToWString(Value.X));
	InputsTextFields[1]->SetText(UHUtilities::FloatToWString(Value.Y));
	InputsTextFields[2]->SetText(UHUtilities::FloatToWString(Value.Z));
	InputsTextFields[3]->SetText(UHUtilities::FloatToWString(Value.W));
}

#endif