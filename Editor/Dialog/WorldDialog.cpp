#include "WorldDialog.h"

#if WITH_EDITOR
#include "../../Runtime/Components/Component.h"
#include "../../../UnheardEngine.h"
#include "../../Runtime/Classes/Types.h"
#include "../../Runtime/Components/Transform.h"
#include "../../Runtime/Renderer/DeferredShadingRenderer.h"

UHWorldDialog::UHWorldDialog(HWND InParentWnd, UHDeferredShadingRenderer* InRenderer)
	: UHDialog(nullptr, InParentWnd)
	, Renderer(InRenderer)
	, CurrentSelected(UHINDEXNONE)
	, WindowHeight(0.0f)
	, CurrComponent(nullptr)
	, bResetWindow(false)
	, bIsSizeChanged(false)
{

}

void UHWorldDialog::ShowDialog()
{
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
}

void UHWorldDialog::Update()
{
	if (bResetWindow && DialogSize.has_value())
	{
		ImGui::SetNextWindowPos(ImVec2(WindowPos.x - DialogSize.value().x, WindowPos.y));
		ImGui::SetNextWindowSize(ImVec2(DialogSize.value().x, WindowHeight));
		bResetWindow = false;
	}

	const std::string WndName = "World Objects";
	const ImVec2 WndSize = ImGui::GetWindowSize();
	bIsSizeChanged = false;

	ImGui::PushStyleColor(ImGuiCol_ResizeGrip, 0);
	ImGui::Begin(WndName.c_str(), nullptr, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResizeGrip);
	ImGui::Text(WndName.c_str());
	if (ImGui::BeginListBox("##", ImVec2(-FLT_MIN, WndSize.y * 0.5f)))
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

	// display detail
	ImGui::NewLine();
	if (CurrComponent)
	{
		CurrComponent->OnGenerateDetailView();
	}

	ImVec2 NewSize = ImGui::GetWindowSize();
	if (DialogSize.has_value() && (DialogSize.value().x != NewSize.x || DialogSize.value().y != NewSize.y))
	{
		bIsSizeChanged = true;
	}
	DialogSize = NewSize;

	ImGui::End();
	ImGui::PopStyleColor();

	ControlSceneObjectSelect();
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
	WindowHeight = static_cast<float>(MainWndRect.bottom - MainWndRect.top);
	bResetWindow = true;
}

ImVec2 UHWorldDialog::GetWindowSize() const
{
	return DialogSize.value();
}

bool UHWorldDialog::IsDialogSizeChanged() const
{
	return bIsSizeChanged;
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
		CurrComponent = Comp;
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