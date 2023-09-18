#include "DetailDialog.h"

#if WITH_EDITOR
#include "../../Editor/Classes/Reflection.h"

UHDetailDialog::UHDetailDialog()
	: UHDetailDialog(nullptr, nullptr)
{

}

UHDetailDialog::UHDetailDialog(HINSTANCE InInstance, HWND InWindow)
	: UHDialog(InInstance, InWindow)
	, OriginDialogRect(RECT())
	, CurrComponent(nullptr)
{

}

void UHDetailDialog::ShowDialog()
{
	if (!IsDialogActive(IDD_DETAILVIEW))
	{
		Dialog = CreateDialog(Instance, MAKEINTRESOURCE(IDD_DETAILVIEW), ParentWindow, (DLGPROC)GDialogProc);
		RegisterUniqueActiveDialog(IDD_DETAILVIEW, this);
		GetWindowRect(Dialog, &OriginDialogRect);

		ResetDialogWindow();
		ShowWindow(Dialog, SW_SHOW);
	}
}

void UHDetailDialog::ResetDialogWindow()
{
	if (!IsDialogActive(IDD_DETAILVIEW))
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
		, MainWndRect.top + DialogHeight
		, DialogWidth
		, DialogHeight
		, 0);
}

BOOL CALLBACK DestroyGUIProc(HWND hwnd, LPARAM lParam)
{
	DestroyWindow(hwnd);
	return TRUE;
}

void UHDetailDialog::GenerateDetailView(UHComponent* InComponent)
{
	// destroy all controls of the previous component
	EnumChildWindows(Dialog, DestroyGUIProc, 0);
	InComponent->OnGenerateDetailView(Dialog);
	CurrComponent = InComponent;
}

#endif