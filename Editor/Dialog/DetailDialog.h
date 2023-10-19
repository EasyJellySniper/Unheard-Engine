#pragma once
#include "Dialog.h"

#if WITH_EDITOR
#include "../../Runtime/Components/Component.h"
#include "../Controls/GUIControl.h"

class UHDetailDialog : public UHDialog
{
public:
	UHDetailDialog(HWND ParentWnd);
	virtual void Update() override;

	void ResetDialogWindow();
	void GenerateDetailView(UHComponent* InComponent);

private:
	ImVec2 WindowPos;
	UHComponent* CurrComponent;
};
#endif