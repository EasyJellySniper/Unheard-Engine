#pragma once
#include "../../../../UnheardEngine.h"
#include "GraphNode.h"

enum class UHMathNodeOperator
{
	Add,
	Subtract,
	Multiply,
	Divide
};

class UHMathNode : public UHGraphNode
{
public:
	UHMathNode(UHMathNodeOperator DefaultOp = UHMathNodeOperator::Add);
	virtual bool CanEvalHLSL() override;
	virtual std::string EvalHLSL(const UHGraphPin* CallerPin) override;
	virtual void InputData(std::ifstream& FileIn) override;
	virtual void OutputData(std::ofstream& FileOut) override;

	void SetOperator(UHMathNodeOperator InOp);
	UHMathNodeOperator GetOperator() const;
	UHGraphPinType GetOutputPinType() const;

private:
	UHMathNodeOperator CurrentOperator;
};