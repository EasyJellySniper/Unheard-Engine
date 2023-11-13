#include "Component.h"

UHComponent::UHComponent()
	: bIsEnabled(true)
#if WITH_EDITOR
	, bIsEditable(true)
#endif
{

}

void UHComponent::SetIsEnabled(bool bInFlag)
{
	bIsEnabled = bInFlag;
}

bool UHComponent::IsEnabled() const
{
	return bIsEnabled;
}

#if WITH_EDITOR
void UHComponent::OnGenerateDetailView()
{
	ImGui::Text("------ Component ------");
	ImGui::Checkbox("Enable", &bIsEnabled);
}

bool UHComponent::IsEditable() const
{
	return bIsEditable;
}
#endif