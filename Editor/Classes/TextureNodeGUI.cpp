#include "TextureNodeGUI.h"

#if WITH_EDITOR
#include "../../../Runtime/Engine/Asset.h"
#include "../../../Runtime/Renderer/DeferredShadingRenderer.h"
#include "EditorUtils.h"
#include "../../../../Runtime/Classes/Utility.h"

UHTexture2DNodeGUI::UHTexture2DNodeGUI(UHAssetManager* InAssetManager, UHDeferredShadingRenderer* InRenderer, UHMaterial* InMat)
	: Node(nullptr)
	, AssetManager(InAssetManager)
	, Renderer(InRenderer)
	, MaterialCache(InMat)
{

}

void UHTexture2DNodeGUI::Update()
{
	const int32_t ItemCount = ComboBox->GetItemCount();
	const int32_t TextureCount = static_cast<int32_t>(AssetManager->GetTexture2Ds().size());

	// refresh the combobox if texture count doesn't match
	if (ItemCount != TextureCount)
	{
		ComboBoxItems.clear();
		for (const UHTexture2D* Tex : AssetManager->GetTexture2Ds())
		{
			const std::wstring Name = UHUtilities::ToStringW(Tex->GetSourcePath());
			ComboBoxItems.push_back(Name);
		}

		const std::wstring PrevSelected = ComboBox->GetSelectedItem();
		ComboBox->Init(PrevSelected, ComboBoxItems);
	}
}

void UHTexture2DNodeGUI::SetDefaultValueFromGUI()
{
	Node->SetSelectedTexturePathName(UHUtilities::ToStringA(ComboBox->GetSelectedItem()));
}

void UHTexture2DNodeGUI::OnSelectCombobox()
{
	SetDefaultValueFromGUI();

	// mark compile flag as bind only if this is connecting to others
	if (Node->GetOutputs()[0]->GetDestPins().size() > 0)
	{
		MaterialCache->SetCompileFlag(UHMaterialCompileFlag::BindOnly);
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
		const std::string Name = Tex->GetSourcePath();
		ComboBoxItems.push_back(UHUtilities::ToStringW(Name));
		TextLength = (std::max)(TextLength, Name.length());
	}

	ComboBoxProp.SetPosX(55).SetPosY(20)
		.SetWidth(static_cast<int32_t>(TextLength) * 5)
		.SetHeight(static_cast<int32_t>(ItemCount) * 5)
		.SetMarginX(30)
		.SetMinVisibleItem(15);
}

void UHTexture2DNodeGUI::PostAddingPins()
{
	const std::wstring TexturePathName = UHUtilities::ToStringW(AssetManager->FindTexturePathName(Node->GetSelectedTexturePathName()));
	ComboBox->Select(TexturePathName);
}

#endif