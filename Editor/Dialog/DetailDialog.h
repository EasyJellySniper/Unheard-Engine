#pragma once
#include "Dialog.h"

#if WITH_EDITOR
#include "../../Runtime/Components/Component.h"
#include "../Controls/GUIControl.h"

class UHDetailDialog : public UHDialog
{
public:
	UHDetailDialog();
	UHDetailDialog(HINSTANCE InInstance, HWND InWindow);

	virtual void ShowDialog() override;

	void ResetDialogWindow();
	void GenerateDetailView(UHComponent* InComponent);

private:
	RECT OriginDialogRect;
	UHComponent* CurrComponent;
};
#endif