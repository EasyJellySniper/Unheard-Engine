#include "InfoDialog.h"

#if WITH_EDITOR

UHInfoDialog::UHInfoDialog(HWND InParentWnd, UHWorldDialog* InWorldDialog)
	: UHDialog(nullptr, InParentWnd)
	, WindowPos(ImVec2())
	, WindowWidth(0.0f)
	, bResetWindow(false)
	, WorldDialog(InWorldDialog)
{

}

void UHInfoDialog::ShowDialog()
{
	UHDialog::ShowDialog();
	ResetDialogWindow();
}

void UHInfoDialog::Update()
{
	if ((bResetWindow || WorldDialog->IsDialogSizeChanged()) && DialogSize.has_value())
	{
		const ImVec2& WorldDialogSize = WorldDialog->GetWindowSize();
		ImGui::SetNextWindowPos(ImVec2(WindowPos.x, WindowPos.y - DialogSize.value().y));
		ImGui::SetNextWindowSize(ImVec2(WindowWidth - WorldDialogSize.x, DialogSize.value().y));
		bResetWindow = false;
	}

	const std::string WndName = "Info Dialog";
	ImGui::PushStyleColor(ImGuiCol_ResizeGrip, 0);
	ImGui::Begin(WndName.c_str(), nullptr, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResizeGrip);

	// implement individual tabs for different contents
	ImGui::BeginTabBar("##");
	if (ImGui::BeginTabItem("Output Log", nullptr))
	{
		if (ImGui::Button("Clear"))
		{
			GLogBuffer.clear();
		}

		if (ImGui::BeginListBox("##", ImVec2(-FLT_MIN, -FLT_MIN)))
		{
			bool bDummy;
			for (int32_t Idx = 0; Idx < static_cast<int32_t>(GLogBuffer.size()); Idx++)
			{
				ImGui::Selectable(GLogBuffer[Idx].c_str(), &bDummy);
			}
			ImGui::EndListBox();
		}

		ImGui::EndTabItem();
	}
	ImGui::EndTabBar();

	DialogSize = ImGui::GetWindowSize();
	ImGui::End();
	ImGui::PopStyleColor();
}

void UHInfoDialog::ResetDialogWindow()
{
	if (!bIsOpened)
	{
		return;
	}

	// always put the dialog on the right side of main window
	RECT MainWndRect;
	GetClientRect(ParentWindow, &MainWndRect);
	ClientToScreen(ParentWindow, (POINT*)&MainWndRect.left);
	ClientToScreen(ParentWindow, (POINT*)&MainWndRect.right);

	WindowPos.x = static_cast<float>(MainWndRect.left);
	WindowPos.y = static_cast<float>(MainWndRect.bottom);
	WindowWidth = static_cast<float>(MainWndRect.right - MainWndRect.left);
	bResetWindow = true;
}

ImVec2 UHInfoDialog::GetWindowSize() const
{
	if (!DialogSize.has_value())
	{
		return ImVec2();
	}
	return DialogSize.value();
}

#endif