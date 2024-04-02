#pragma once
#include "../../UnheardEngine.h"

#if WITH_EDITOR
#include "GraphNodeGUI.h"
#include "../../../Runtime/Classes/GraphNode/ParameterNode.h"

class UHDeferredShadingRenderer;
class UHMaterial;

class UHFloatNodeGUI : public UHGraphNodeGUI
{
public:
	UHFloatNodeGUI(UHDeferredShadingRenderer* InRenderer, UHMaterial* InMat);
	virtual void SetDefaultValueFromGUI() override;
	virtual void Update() override;

protected:
	virtual void PreAddingPins() override;
	virtual void PostAddingPins() override;

private:
	UHFloatNode* Node;
	UHDeferredShadingRenderer* Renderer;
	UHMaterial* MaterialCache;
};

class UHFloat2NodeGUI : public UHGraphNodeGUI
{
public:
	UHFloat2NodeGUI(UHDeferredShadingRenderer* InRenderer, UHMaterial* InMat);
	virtual void SetDefaultValueFromGUI() override;
	virtual void Update() override;

protected:
	virtual void PreAddingPins() override;
	virtual void PostAddingPins() override;

private:
	UHFloat2Node* Node;
	UHDeferredShadingRenderer* Renderer;
	UHMaterial* MaterialCache;
};

class UHFloat3NodeGUI : public UHGraphNodeGUI
{
public:
	UHFloat3NodeGUI(UHDeferredShadingRenderer* InRenderer, UHMaterial* InMat);
	virtual void SetDefaultValueFromGUI() override;
	virtual void Update() override;

protected:
	virtual void PreAddingPins() override;
	virtual void PostAddingPins() override;

private:
	UHFloat3Node* Node;
	UHDeferredShadingRenderer* Renderer;
	UHMaterial* MaterialCache;
};

class UHFloat4NodeGUI : public UHGraphNodeGUI
{
public:
	UHFloat4NodeGUI(UHDeferredShadingRenderer* InRenderer, UHMaterial* InMat);
	virtual void SetDefaultValueFromGUI() override;
	virtual void Update() override;

protected:
	virtual void PreAddingPins() override;
	virtual void PostAddingPins() override;

private:
	UHFloat4Node* Node;
	UHDeferredShadingRenderer* Renderer;
	UHMaterial* MaterialCache;
};

#endif