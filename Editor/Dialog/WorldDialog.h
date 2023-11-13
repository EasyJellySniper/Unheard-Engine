#pragma once
#include "Dialog.h"

#if WITH_EDITOR
#include "../../Runtime/Components/Component.h"
#include <optional>

class UHDeferredShadingRenderer;

class UHWorldDialog : public UHDialog
{
public:
	UHWorldDialog(HWND ParentWnd, UHDeferredShadingRenderer* InRenderer);

	virtual void ShowDialog() override;
	virtual void Update() override;

	void ResetDialogWindow();
	ImVec2 GetWindowSize() const;
	bool IsDialogSizeChanged() const;

private:
	void ControlSceneObjectSelect();
	void ControlSceneObjectDoubleClick();

	ImVec2 WindowPos;
	std::optional<ImVec2> DialogSize;
	float WindowHeight;
	std::vector<UHComponent*> SceneObjects;
	int32_t CurrentSelected;
	bool bResetWindow;
	bool bIsSizeChanged;

	UHDeferredShadingRenderer* Renderer;
	UHComponent* CurrComponent;
};

#endif