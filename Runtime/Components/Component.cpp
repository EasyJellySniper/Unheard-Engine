#include "Component.h"

UHComponent::UHComponent()
	: bIsEnabled(true)
{

}

void UHComponent::OnSave(std::ofstream& OutStream)
{
	UHObject::OnSave(OutStream);
	OutStream.write(reinterpret_cast<const char*>(&bIsEnabled), sizeof(bIsEnabled));
}

void UHComponent::OnLoad(std::ifstream& InStream)
{
	UHObject::OnLoad(InStream);
	InStream.read(reinterpret_cast<char*>(&bIsEnabled), sizeof(bIsEnabled));
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