#pragma once
#include "../../UnheardEngine.h"

#if WITH_DEBUG
#include "GraphNodeGUI.h"
#include "../../Runtime/Classes/GraphNode/TextureNode.h"

class UHAssetManager;
class UHMaterial;

class UHTexture2DNodeGUI : public UHGraphNodeGUI
{
public:
	UHTexture2DNodeGUI(UHAssetManager* InAssetManager, UHMaterial* InMat);
	virtual void SetDefaultValueFromGUI() override;
	virtual void OnSelectCombobox() override;

protected:
	virtual void PreAddingPins() override;
	virtual void PostAddingPins() override;

private:
	UHTexture2DNode* Node;
	UHAssetManager* AssetManager;
	UHMaterial* MaterialCache;
};

#endif