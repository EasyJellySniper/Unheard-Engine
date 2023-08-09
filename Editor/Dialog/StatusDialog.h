#pragma once
#include "Dialog.h"

// simple dialog for showing the status, such as "Loading, Processing..etc"
// for now it's simply used in scope
class UHStatusDialogScope : public UHDialog
{
public:
	UHStatusDialogScope(HINSTANCE InInstance, HWND InParentWnd, std::string InMsg);
	~UHStatusDialogScope();

	virtual void ShowDialog() {}
};