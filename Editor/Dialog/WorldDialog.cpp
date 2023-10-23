#include "WorldDialog.h"

#if WITH_EDITOR
#include "../../Runtime/Components/Component.h"
#include "../../../UnheardEngine.h"
#include "../../Runtime/Classes/Types.h"
#include "../../Runtime/Components/Transform.h"
#include "../../Runtime/Renderer/DeferredShadingRenderer.h"

UHWorldDialog::UHWorldDialog(HWND InParentWnd, UHDeferredShadingRenderer* InRenderer, UHDetailDialog* InDetailView)
	: UHDialog(nullptr, InParentWnd)
	, Renderer(InRenderer)
	, DetailView(InDetailView)
	, CurrentSelected(UHINDEXNONE)
{

}

void UHWorldDialog::ShowDialog()
{
	if (bIsOpened)
	{
		return;
	}

	UHDialog::ShowDialog();
	CurrentSelected = UHINDEXNONE;
	ResetDialogWindow();

	SceneObjects.clear();
	const std::vector<UHComponent*>& Comps = GetComponents<UHComponent>();
	for (UHComponent* Comp : Comps)
	{
		if (Comp->IsEditable())
		{
			SceneObjects.push_back(Comp);
		}
	}

	if (DetailView)
	{
		DetailView->ShowDialog();
		DetailView->ResetDialogWindow();
	}
}

void UHWorldDialog::Update()
{
	if (!bIsOpened)
	{
		return;
	}

	const std::string WndName = "World Objects";
	ImGui::Begin(WndName.c_str(), &bIsOpened, ImGuiWindowFlags_HorizontalScrollbar);
	ImGui::SetWindowPos(WndName.c_str(), WindowPos);

	ImVec2 WndSize = ImGui::GetWindowSize();
	WndSize.x -= 10.0f;
	WndSize.y -= 50.0f;

	if (ImGui::BeginListBox("ObjectList", WndSize))
	{
		for (int32_t Idx = 0; Idx < static_cast<int32_t>(SceneObjects.size()); Idx++)
		{
			bool bIsSelected = (CurrentSelected == Idx);
			if (ImGui::Selectable(SceneObjects[Idx]->GetName().c_str(), &bIsSelected, ImGuiSelectableFlags_AllowDoubleClick))
			{
				CurrentSelected = Idx;
				if (ImGui::IsMouseDoubleClicked(0))
				{
					ControlSceneObjectDoubleClick();
				}
			}
		}
		ImGui::EndListBox();
	}
	ImGui::End();

	ControlSceneObjectSelect();

	if (!bIsOpened)
	{
		OnFinished();
	}
}

void UHWorldDialog::ResetDialogWindow()
{
	if (!bIsOpened)
	{
		return;
	}

	// always put the dialog on the right side of main window
	RECT MainWndRect;
	GetClientRect(ParentWindow, &MainWndRect);
	ClientToScreen(ParentWindow, (POINT*)&MainWndRect.left);
	ClientToScreen(ParentWindow, (POINT*)&MainWndRect.right);

	WindowPos.x = static_cast<float>(MainWndRect.right);
	WindowPos.y = static_cast<float>(MainWndRect.top);
}

void UHWorldDialog::OnFinished()
{
	if (UHScene* Scene = Renderer->GetCurrentScene())
	{
		Scene->SetCurrentSelectedComponent(nullptr);
	}

	if (DetailView != nullptr)
	{
		DetailView->Close();
	}
}

void UHWorldDialog::ControlSceneObjectSelect()
{
	if (CurrentSelected == UHINDEXNONE)
	{
		return;
	}

	UHComponent* Comp = SceneObjects[CurrentSelected];
	if (Comp == nullptr)
	{
		return;
	}

	// set current selected component to scene
	if (UHScene* Scene = Renderer->GetCurrentScene())
	{
		if (Scene->GetCurrentSelectedComponent() == Comp)
		{
			return;
		}
		Scene->SetCurrentSelectedComponent(Comp);
	}

	if (DetailView != nullptr)
	{
		DetailView->GenerateDetailView(Comp);
	}
}

void UHWorldDialog::ControlSceneObjectDoubleClick()
{
	const UHTransformComponent* Comp = dynamic_cast<UHTransformComponent*>(SceneObjects[CurrentSelected]);

	if (Comp == nullptr)
	{
		return;
	}

	// get main camera and teleport to the double-clicked object
	if (UHScene* Scene = Renderer->GetCurrentScene())
	{
		if (UHCameraComponent* Camera = Scene->GetMainCamera())
		{
			// do not teleport to main camera itself
			if (Comp != Camera)
			{
				// when it's a renderer component, it needs to use the center of bound instead
				if (const UHMeshRendererComponent* MeshComp = dynamic_cast<const UHMeshRendererComponent*>(Comp))
				{
					Camera->SetPosition(MeshComp->GetRendererBound().Center - Camera->GetForward() * 10.0f + XMFLOAT3(0, 1, 0));
				}
				else
				{
					Camera->SetPosition(Comp->GetPosition() - Camera->GetForward() * 10.0f + XMFLOAT3(0, 1, 0));
				}
			}
		}
	}
}

#endif