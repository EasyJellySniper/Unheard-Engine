#pragma once
#include "../../UnheardEngine.h"

#if WITH_EDITOR
#include "GraphNodeGUI.h"
#include "Runtime/Classes/GraphNode/TextureNode.h"

class UHAssetManager;
class UHDeferredShadingRenderer;
class UHMaterial;

class UHTexture2DNodeGUI : public UHGraphNodeGUI
{
public:
	UHTexture2DNodeGUI(UHAssetManager* InAssetManager, UHDeferredShadingRenderer* InRenderer, UHMaterial* InMat);
	virtual void Update() override;
	virtual void SetDefaultValueFromGUI() override;
	virtual void OnSelectCombobox() override;

protected:
	virtual void PreAddingPins() override;
	virtual void PostAddingPins() override;

private:
	UHTexture2DNode* Node;
	UHAssetManager* AssetManager;
	UHDeferredShadingRenderer* Renderer;
	UHMaterial* MaterialCache;
};

#endif