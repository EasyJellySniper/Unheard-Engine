#include "GraphNode.h"
#if WITH_EDITOR
#include "../../../Editor/Classes/GraphNodeGUI.h"
#endif

UHGraphNode::UHGraphNode(bool bInCanBeDeleted)
	: Name("")
#if WITH_EDITOR
	, GUICache(nullptr)
	, bIsCompilingRayTracing(false)
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

#if WITH_EDITOR
void UHGraphNode::SetGUI(UHGraphNodeGUI* InGUI)
{
	GUICache = InGUI;
}

UHGraphNodeGUI* UHGraphNode::GetGUI() const
{
	return GUICache;
}
#endif

void UHGraphNode::SetIsCompilingRayTracing(bool bInFlag)
{
	bIsCompilingRayTracing = bInFlag;
}

std::string UHGraphNode::GetName() const
{
	return Name;
}

UHGraphNodeType UHGraphNode::GetType() const
{
	return NodeType;
}

std::vector<UniquePtr<UHGraphPin>>& UHGraphNode::GetInputs()
{
	return Inputs;
}

std::vector<UniquePtr<UHGraphPin>>& UHGraphNode::GetOutputs()
{
	return Outputs;
}

bool UHGraphNode::CanBeDeleted() const
{
	return bCanBeDeleted;
}