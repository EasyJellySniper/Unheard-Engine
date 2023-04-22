#include "GraphNode.h"
#if WITH_DEBUG
#include "../../../Editor/Classes/GraphNodeGUI.h"
#endif

UHGraphNode::UHGraphNode(bool bInCanBeDeleted)
	: Name("")
#if WITH_DEBUG
	, GUICache(nullptr)
#endif
	, bCanBeDeleted(bInCanBeDeleted)
	, NodeType(UHGraphNodeType::UnknownNode)
{

}

UHGraphNode::~UHGraphNode()
{
	for (auto& Pin : Inputs)
	{
		Pin.reset();
	}

	for (auto& Pin : Outputs)
	{
		Pin.reset();
	}
}

#if WITH_DEBUG
void UHGraphNode::SetGUI(UHGraphNodeGUI* InGUI)
{
	GUICache = InGUI;
}

UHGraphNodeGUI* UHGraphNode::GetGUI() const
{
	return GUICache;
}
#endif

std::string UHGraphNode::GetName() const
{
	return Name;
}

UHGraphNodeType UHGraphNode::GetType() const
{
	return NodeType;
}

std::vector<std::unique_ptr<UHGraphPin>>& UHGraphNode::GetInputs()
{
	return Inputs;
}

std::vector<std::unique_ptr<UHGraphPin>>& UHGraphNode::GetOutputs()
{
	return Outputs;
}

bool UHGraphNode::CanBeDeleted() const
{
	return bCanBeDeleted;
}