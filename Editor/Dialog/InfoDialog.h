#pragma once
#include "Dialog.h"

#if WITH_EDITOR
#include "WorldDialog.h"
#include <optional>

// the information dialog that is at the bottom part of the window
class UHInfoDialog : public UHDialog
{
public:
	UHInfoDialog(HWND ParentWnd, UHWorldDialog* InWorldDialog);

	virtual void ShowDialog() override;
	virtual void Update() override;

	void ResetDialogWindow();
	ImVec2 GetWindowSize() const;

private:
	ImVec2 WindowPos;
	std::optional<ImVec2> DialogSize;
	float WindowWidth;
	bool bResetWindow;
	UHWorldDialog* WorldDialog;
};

#endif