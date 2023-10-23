#include "StatusDialog.h"
#include "../Classes/EditorUtils.h"
#include "../../resource.h"

UHStatusDialogScope::UHStatusDialogScope(std::string InMsg)
	: UHDialog(nullptr, nullptr)
{
	// doesn't need a callback
	Dialog = CreateDialog(nullptr, MAKEINTRESOURCE(IDD_STATUS), nullptr, (DLGPROC)GDialogProc);
    ShowWindow(Dialog, SW_SHOW);
	SetWindowTextA(GetDlgItem(Dialog, IDC_STATUS_TEXT), InMsg.c_str());
}

UHStatusDialogScope::~UHStatusDialogScope()
{
	DestroyWindow(Dialog);
}