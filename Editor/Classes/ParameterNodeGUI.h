#pragma once
#include "../../UnheardEngine.h"

#if WITH_DEBUG
#include "GraphNodeGUI.h"
#include "../../Runtime/Classes/GraphNode/ParameterNode.h"

class UHFloatNodeGUI : public UHGraphNodeGUI
{
public:
	UHFloatNodeGUI();
	virtual void SetDefaultValueFromGUI() override;

protected:
	virtual void PreAddingPins() override;
	virtual void PostAddingPins() override;

private:
	UHFloatNode* Node;
};

class UHFloat2NodeGUI : public UHGraphNodeGUI
{
public:
	UHFloat2NodeGUI();
	virtual void SetDefaultValueFromGUI() override;

protected:
	virtual void PreAddingPins() override;
	virtual void PostAddingPins() override;

private:
	UHFloat2Node* Node;
};

class UHFloat3NodeGUI : public UHGraphNodeGUI
{
public:
	UHFloat3NodeGUI();
	virtual void SetDefaultValueFromGUI() override;

protected:
	virtual void PreAddingPins() override;
	virtual void PostAddingPins() override;

private:
	UHFloat3Node* Node;
};

class UHFloat4NodeGUI : public UHGraphNodeGUI
{
public:
	UHFloat4NodeGUI();
	virtual void SetDefaultValueFromGUI() override;

protected:
	virtual void PreAddingPins() override;
	virtual void PostAddingPins() override;

private:
	UHFloat4Node* Node;
};

#endif