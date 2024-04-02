#pragma once
#include "Dialog.h"

#if WITH_EDITOR
#include "../../../Runtime/Classes/GraphNode/MaterialNode.h"
#include "../Classes/GraphNodeGUI.h"
#include "../Classes/MenuGUI.h"
#include <memory>
#include "../Controls/ListBox.h"
#include "../Controls/Button.h"
#include "../Controls/GroupBox.h"
#include "../Controls/Label.h"
#include "../Controls/ComboBox.h"
#include <optional>

// for drawing lines
#include <gdiplus.h>
using namespace Gdiplus;
#pragma comment (lib,"Gdiplus.lib")

class UHAssetManager;
class UHDeferredShadingRenderer;
class UHMaterial;

class UHMaterialNodeGUI : public UHGraphNodeGUI
{
public:
	virtual void Update() override {}

private:
	virtual void PreAddingPins() override {}
	virtual void PostAddingPins() override {}
};

// material dialog class
class UHMaterialDialog : public UHDialog
{
public:
	UHMaterialDialog(HINSTANCE InInstance, HWND InWindow, UHAssetManager* InAssetManager, UHDeferredShadingRenderer* InRenderer);
	~UHMaterialDialog();

	virtual void ShowDialog() override;
	virtual void Update(bool& bIsDialogActive) override;
	void ResetDialogWindow();

	void Init();
	void CreateWorkAreaMemDC(int32_t Width, int32_t Height);
	void RecompileMaterial(int32_t MatIndex);
	void ResaveMaterial(int32_t MatIndex);
	void ResaveAllMaterials();

private:
	// dialog callbacks
	void OnFinished();
	void OnMenuHit(uint32_t wParamLow);
	void OnResizing();
	void OnDrawing(HDC Hdc);

	// material editing
	void SelectMaterial(int32_t MatIndex);
	void TryAddNodes(UHGraphNode* InputNode = nullptr, POINT GUIRelativePos = POINT());
	void TryDeleteNodes();
	void TryDisconnectPin();
	void TryMoveNodes();
	void TryConnectNodes();
	void ProcessPopMenu();
	void DrawPinConnectionLine(bool bIsErasing = false);

	// controls
	UniquePtr<UHGroupBox> WorkAreaGUI;

	void ControlRecompileMaterial();
	void ControlResaveMaterial();
	void ControlResaveAllMaterials();
	void ControlCullMode(const int32_t Idx);
	void ControlBlendMode(const int32_t Idx);

	// all node GUI, the first element is material GUI
	std::vector<UniquePtr<UHGraphNodeGUI>> EditNodeGUIs;

	UHMenuGUI NodeFunctionMenu;
	UHMenuGUI NodePinMenu;
	UHMenuGUI AddNodeMenu;
	UHMenuGUI ParameterMenu;
	UHMenuGUI TextureMenu;
	UHGraphNode* NodeToDelete;
	UHGraphPin* PinToDisconnect;
	int32_t NodeMenuAction;
	bool bNeedRepaint;

	int32_t CurrentMaterialIndex;
	UHMaterial* CurrentMaterial;

	HWND GUIToMove;

	POINT MousePos;
	POINT MousePosWhenRightDown;
	POINT PrevMousePos;
	bool bIsUpdatingDragLine;
	RECT DragRect;

	ImVec2 WindowPos;
	std::optional<ImVec2> DialogSize;
	float WindowWidth;
	float WindowHeight;
	bool bResetWindow;

	UHAssetManager* AssetManager;
	UHDeferredShadingRenderer* Renderer;

	// GDI stuff
	GdiplusStartupInput GdiplusStartupInput;
	ULONG_PTR GdiplusToken;
	HDC WorkAreaMemDC;
	HBITMAP WorkAreaBmp;
};

#endif