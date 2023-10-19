#include "DetailDialog.h"

#if WITH_EDITOR
#include "../../Editor/Classes/Reflection.h"

UHDetailDialog::UHDetailDialog(HWND ParentWnd)
	: UHDialog(nullptr, ParentWnd)
	, CurrComponent(nullptr)
{

}

void UHDetailDialog::Update()
{
	if (!bIsOpened)
	{
		return;
	}

	const std::string WndName = "Details";
	ImGui::Begin(WndName.c_str(), nullptr, ImGuiWindowFlags_HorizontalScrollbar);
	ImGui::SetWindowPos(WndName.c_str(), WindowPos);

	if (CurrComponent)
	{
		CurrComponent->OnGenerateDetailView();
	}

	ImGui::End();
}

void UHDetailDialog::ResetDialogWindow()
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
	const int32_t DialogHeight = (MainWndRect.bottom - MainWndRect.top) / 2;

	WindowPos.x = static_cast<float>(MainWndRect.right);
	WindowPos.y = static_cast<float>(MainWndRect.top + DialogHeight);
}

void UHDetailDialog::GenerateDetailView(UHComponent* InComponent)
{
	CurrComponent = InComponent;
}

#endif