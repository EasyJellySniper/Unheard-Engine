#pragma once
#include "../../UnheardEngine.h"

#if WITH_EDITOR

class UHMenuGUI
{
public:
	UHMenuGUI();
	~UHMenuGUI();

	void InsertOption(std::string InOption, uint32_t InID = 0, HMENU InSubMenu = nullptr);
	void ShowMenu(HWND CallbackWnd, uint32_t X, uint32_t Y);
	void SetOptionActive(int32_t Index, bool bActive);

	HMENU GetMenu() const;

private:
	HMENU MenuHandle;
	int32_t OptionCount;
};

#endif