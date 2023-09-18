#pragma once
#include "../../UnheardEngine.h"

#if WITH_EDITOR
#include "GraphNodeGUI.h"
#include "../../Runtime/Classes/GraphNode/MathNode.h"

class UHMathNodeGUI : public UHGraphNodeGUI
{
public:
	UHMathNodeGUI();
	virtual void SetDefaultValueFromGUI() override;

protected:
	virtual void PreAddingPins() override;
	virtual void PostAddingPins() override;

private:
	UHMathNode* Node;
};

#endif