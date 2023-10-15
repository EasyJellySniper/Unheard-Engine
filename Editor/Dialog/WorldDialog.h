#pragma once
#include "Dialog.h"

#if WITH_EDITOR
#include "../Controls/ListBox.h"
#include "../../Runtime/Components/Component.h"
#include "DetailDialog.h"

class UHDeferredShadingRenderer;

class UHWorldDialog : public UHDialog
{
public:
	UHWorldDialog(HINSTANCE InInstance, HWND InWindow, UHDeferredShadingRenderer* InRenderer, UHDetailDialog* InDetailView);

	virtual void ShowDialog() override;
	void ResetDialogWindow();

private:
	void OnFinished();
	void ControlSceneObjectSelect();
	void ControlSceneObjectDoubleClick();

	RECT OriginDialogRect;
	UniquePtr<UHListBox> SceneObjectListGUI;
	std::vector<UHComponent*> SceneObjects;

	UHDeferredShadingRenderer* Renderer;
	UHDetailDialog* DetailView;
};

#endif