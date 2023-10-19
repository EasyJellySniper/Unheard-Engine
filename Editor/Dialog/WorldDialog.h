#pragma once
#include "Dialog.h"

#if WITH_EDITOR
#include "../../Runtime/Components/Component.h"
#include "DetailDialog.h"

class UHDeferredShadingRenderer;

class UHWorldDialog : public UHDialog
{
public:
	UHWorldDialog(HWND ParentWnd, UHDeferredShadingRenderer* InRenderer, UHDetailDialog* InDetailView);

	virtual void ShowDialog() override;
	virtual void Update() override;
	void ResetDialogWindow();

private:
	void OnFinished();
	void ControlSceneObjectSelect();
	void ControlSceneObjectDoubleClick();

	ImVec2 WindowPos;
	std::vector<UHComponent*> SceneObjects;
	int32_t CurrentSelected;

	UHDeferredShadingRenderer* Renderer;
	UHDetailDialog* DetailView;
};

#endif