#include "Component.h"

UHComponent::UHComponent()
#if WITH_EDITOR
	: bIsEditable(true)
	, DetailStartHeight(0)
#endif
{

}

#if WITH_EDITOR
bool UHComponent::IsEditable() const
{
	return bIsEditable;
}
#endif