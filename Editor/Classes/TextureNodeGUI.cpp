#include "TextureNodeGUI.h"

#if WITH_DEBUG
#include "../../Runtime/Engine/Asset.h"
#include "../../Runtime/Renderer/DeferredShadingRenderer.h"
#include "EditorUtils.h"

UHTexture2DNodeGUI::UHTexture2DNodeGUI(UHAssetManager* InAssetManager, UHDeferredShadingRenderer* InRenderer, UHMaterial* InMat)
	: Node(nullptr)
	, AssetManager(InAssetManager)
	, Renderer(InRenderer)
	, MaterialCache(InMat)
{

}

void UHTexture2DNodeGUI::SetDefaultValueFromGUI()
{
	Node->SetSelectedTextureName(UHEditorUtil::GetComboBoxSelectedText(ComboBox));
}

void UHTexture2DNodeGUI::OnSelectCombobox()
{
	SetDefaultValueFromGUI();

	// mark compile flag as bind only if this is connecting to others
	if (Node->GetOutputs()[0]->GetDestPins().size() > 0)
	{
		MaterialCache->SetCompileFlag(BindOnly);
		MaterialCache->SetRenderDirties(true);
		Renderer->RefreshMaterialShaders(MaterialCache);
	}
}

void UHTexture2DNodeGUI::PreAddingPins()
{
	Node = static_cast<UHTexture2DNode*>(GraphNode);

	size_t TextLength = 0;
	size_t ItemCount = AssetManager->GetTexture2Ds().size();
	for (const UHTexture2D* Tex : AssetManager->GetTexture2Ds())
	{
		const std::string Name = Tex->GetName();
		ComboBoxItems.push_back(Name);
		TextLength = (std::max)(TextLength, Name.length());
	}

	ComboBoxMeasure.PosX = 55;
	ComboBoxMeasure.PosY = 20;
	ComboBoxMeasure.Width = static_cast<int32_t>(TextLength) * 5;
	ComboBoxMeasure.Height = static_cast<int32_t>(ItemCount) * 5;
	ComboBoxMeasure.Margin = 30;
}

void UHTexture2DNodeGUI::PostAddingPins()
{
	UHEditorUtil::SelectComboBox(ComboBox, Node->GetSelectedTextureName());
}

#endif