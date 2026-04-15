#include "Component.h"

UHComponent::UHComponent()
	: bIsEnabled(true)
{

}

void UHComponent::OnSave(std::ofstream& OutStream)
{
	UHObject::OnSave(OutStream);
	UHUtilities::WriteBoolData(OutStream, bIsEnabled);
}

void UHComponent::OnLoad(std::ifstream& InStream)
{
	UHObject::OnLoad(InStream);
	UHUtilities::ReadBoolData(InStream, bIsEnabled);
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
	if (ImGui::Checkbox("Enable", &bIsEnabled))
	{
		OnActivityChanged();
	}
}
#endif