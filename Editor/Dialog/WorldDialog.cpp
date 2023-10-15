#include "WorldDialog.h"

#if WITH_EDITOR
#include "../../Runtime/Components/Component.h"
#include "../../../UnheardEngine.h"
#include "../../Runtime/Classes/Types.h"
#include "../../Runtime/Components/Transform.h"
#include "../../Runtime/Renderer/DeferredShadingRenderer.h"

UHWorldDialog::UHWorldDialog(HINSTANCE InInstance, HWND InWindow, UHDeferredShadingRenderer* InRenderer, UHDetailDialog* InDetailView)
	: UHDialog(InInstance, InWindow)
	, OriginDialogRect(RECT())
	, Renderer(InRenderer)
	, DetailView(InDetailView)
{

}

void UHWorldDialog::ShowDialog()
{
	if (!IsDialogActive(IDD_WORLDEDITOR))
	{
		Dialog = CreateDialog(Instance, MAKEINTRESOURCE(IDD_WORLDEDITOR), ParentWindow, (DLGPROC)GDialogProc);
		RegisterUniqueActiveDialog(IDD_WORLDEDITOR, this);
		GetWindowRect(Dialog, &OriginDialogRect);

		// Dialog callbacks
		OnDestroy.clear();
		OnDestroy.push_back(StdBind(&UHWorldDialog::OnFinished, this));

		// scene list GUI
		SceneObjectListGUI = MakeUnique<UHListBox>(GetDlgItem(Dialog, IDC_SCENEOBJ_LIST), UHGUIProperty().SetAutoSize(AutoSizeY));
		SceneObjectListGUI->Reset();
		SceneObjectListGUI->OnSelected.push_back(StdBind(&UHWorldDialog::ControlSceneObjectSelect, this));
		SceneObjectListGUI->OnDoubleClicked.push_back(StdBind(&UHWorldDialog::ControlSceneObjectDoubleClick, this));
		SceneObjects.clear();

		// add components to the GUI list
		const std::vector<UHComponent*>& Comps = GetComponents<UHComponent>();
		for (UHComponent* Comp : Comps)
		{
			if (Comp->IsEditable())
			{
				SceneObjectListGUI->AddItem(Comp->GetName());
				SceneObjects.push_back(Comp);
			}
		}

		ResetDialogWindow();
		ShowWindow(Dialog, SW_SHOW);
	}
}

void UHWorldDialog::ResetDialogWindow()
{
	if (!IsDialogActive(IDD_WORLDEDITOR))
	{
		return;
	}

	// always put the dialog on the right side of main window
	RECT MainWndRect;
	GetClientRect(ParentWindow, &MainWndRect);
	ClientToScreen(ParentWindow, (POINT*)&MainWndRect.left);
	ClientToScreen(ParentWindow, (POINT*)&MainWndRect.right);

	const int32_t DialogWidth = OriginDialogRect.right - OriginDialogRect.left;
	const int32_t DialogHeight = (MainWndRect.bottom - MainWndRect.top) / 2;

	SetWindowPos(Dialog, nullptr, MainWndRect.right
		, MainWndRect.top
		, DialogWidth
		, DialogHeight
		, 0);
}

void UHWorldDialog::OnFinished()
{
	if (UHScene* Scene = Renderer->GetCurrentScene())
	{
		Scene->SetCurrentSelectedComponent(nullptr);
	}

	if (DetailView != nullptr)
	{
		SendMessage(DetailView->GetDialog(), WM_COMMAND, MAKEWPARAM(IDCANCEL, 0), 0);
	}
}

void UHWorldDialog::ControlSceneObjectSelect()
{
	const int32_t SelectedIdx = SceneObjectListGUI->GetSelectedIndex();
	UHComponent* Comp = SceneObjects[SelectedIdx];
	if (Comp == nullptr)
	{
		return;
	}

	// set current selected component to scene
	if (UHScene* Scene = Renderer->GetCurrentScene())
	{
		Scene->SetCurrentSelectedComponent(Comp);
	}

	if (DetailView != nullptr)
	{
		DetailView->GenerateDetailView(Comp);
	}
}

void UHWorldDialog::ControlSceneObjectDoubleClick()
{
	const int32_t SelectedIdx = SceneObjectListGUI->GetSelectedIndex();
	const UHTransformComponent* Comp = dynamic_cast<UHTransformComponent*>(SceneObjects[SelectedIdx]);

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