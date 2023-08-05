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
class UHDeferredShadingRenderer;
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
	UHMaterialDialog(HINSTANCE InInstance, HWND InWindow, UHAssetManager* InAssetManager, UHDeferredShadingRenderer* InRenderer);
	~UHMaterialDialog();

	virtual void ShowDialog() override;
	virtual void Update() override;

	void Init();
	void RecompileMaterial(int32_t MatIndex);
	void ResaveMaterial(int32_t MatIndex);
	void ResaveAllMaterials();

private:
	void SelectMaterial(int32_t MatIndex);
	void TryAddNodes(UHGraphNode* InputNode = nullptr, POINT GUIRelativePos = POINT());
	void TryDeleteNodes();
	void TryDisconnectPin();
	void TryMoveNodes();
	void TryConnectNodes();
	void ProcessPopMenu();
	void DrawPinConnectionLine();

	// control functions
	void ControlRecompileMaterial();
	void ControlResaveMaterial();
	void ControlResaveAllMaterials();
	void ControlCullMode();
	void ControlBlendMode();

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
	UHDeferredShadingRenderer* Renderer;

	// GDI stuff
	GdiplusStartupInput GdiplusStartupInput;
	ULONG_PTR GdiplusToken;

	// declare function pointer type for editor control
	std::unordered_map<int32_t, void(UHMaterialDialog::*)()> ControlCallbacks;
};

#endif