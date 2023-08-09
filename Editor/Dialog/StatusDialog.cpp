#include "StatusDialog.h"
#include "../Classes/EditorUtils.h"
#include "../../resource.h"

UHStatusDialogScope::UHStatusDialogScope(HINSTANCE InInstance, HWND InParentWnd, std::string InMsg)
	: UHDialog(InInstance, InParentWnd)
{
	// doesn't need a callback
	Dialog = CreateDialog(Instance, MAKEINTRESOURCE(IDD_STATUS), ParentWindow, (DLGPROC)GDialogProc);
    ShowWindow(Dialog, SW_SHOW);
	SetWindowTextA(GetDlgItem(Dialog, IDC_STATUS_TEXT), InMsg.c_str());
}

UHStatusDialogScope::~UHStatusDialogScope()
{
	DestroyWindow(Dialog);
}