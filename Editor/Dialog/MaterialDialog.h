#pragma once
#include "Dialog.h"

#if WITH_DEBUG
#include "../../Runtime/Classes/GraphNode/MaterialNode.h"
#include "../Classes/GraphNodeGUI.h"
#include "../Classes/MenuGUI.h"
#include <memory>

// for drawing lines
#include <gdiplus.h>
using namespace Gdiplus;
#pragma comment (lib,"Gdiplus.lib")

class UHAssetManager;
class UHMaterial;

class UHMaterialNodeGUI : public UHGraphNodeGUI
{
	virtual void PreAddingPins() override {}
	virtual void PostAddingPins() override {}
};

// material dialog class
class UHMaterialDialog : public UHDialog
{
public:
	UHMaterialDialog();
	UHMaterialDialog(HINSTANCE InInstance, HWND InWindow, UHAssetManager* InAssetManager);
	~UHMaterialDialog();

	virtual void ShowDialog() override;
	virtual void Update() override;

	void Init();

private:
	void SelectMaterial(int32_t MatIndex);
	void TryAddNodes(UHGraphNode* InputNode = nullptr, POINT GUIRelativePos = POINT());
	void TryDeleteNodes();
	void TryDisconnectPin();
	void TryMoveNodes();
	void TryConnectNodes();
	void ProcessPopMenu();
	void DrawPinConnectionLine();
	void RecompileMaterial();
	void ResaveMaterial();

	// all node GUI, the first element is material GUI
	std::vector<std::unique_ptr<UHGraphNodeGUI>> EditNodeGUIs;

	UHMenuGUI NodeFunctionMenu;
	UHMenuGUI NodePinMenu;
	UHMenuGUI AddNodeMenu;
	UHMenuGUI ParameterMenu;
	UHMenuGUI TextureMenu;
	UHGraphNode* NodeToDelete;
	UHGraphPin* PinToDisconnect;
	int32_t CurrentMaterialIndex;
	UHMaterial* CurrentMaterial;

	HWND GUIToMove;

	POINT MousePos;
	POINT MousePosWhenRightDown;

	UHAssetManager* AssetManager;

	// GDI stuff
	GdiplusStartupInput GdiplusStartupInput;
	ULONG_PTR GdiplusToken;
};

#endif