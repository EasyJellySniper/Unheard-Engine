#include "MenuGUI.h"

#if WITH_DEBUG

UHMenuGUI::UHMenuGUI()
	: MenuHandle(nullptr)
    , OptionCount(0)
{
	MenuHandle = CreatePopupMenu();
}

UHMenuGUI::~UHMenuGUI()
{
	DestroyMenu(MenuHandle);
}

void UHMenuGUI::InsertOption(std::string InOption, uint32_t InID, HMENU InSubMenu)
{
    MENUITEMINFOA MenuItemInfo{};
    MenuItemInfo.cbSize = sizeof(MENUITEMINFO);
    MenuItemInfo.fMask = MIIM_STRING;
    MenuItemInfo.fType = MFT_STRING;
    MenuItemInfo.fState = MFS_DEFAULT;
    MenuItemInfo.dwTypeData = (LPSTR)InOption.c_str();
    MenuItemInfo.cch = static_cast<uint32_t>(InOption.length());

    if (InSubMenu)
    {
        MenuItemInfo.fMask |= MIIM_SUBMENU;
        MenuItemInfo.hSubMenu = InSubMenu;
    }

    if (InID > 0)
    {
        MenuItemInfo.fMask |= MIIM_ID;
        MenuItemInfo.wID = InID;
    }

    InsertMenuItemA(MenuHandle, OptionCount, true, &MenuItemInfo);
    OptionCount++;
}

void UHMenuGUI::ShowMenu(HWND CallbackWnd, uint32_t X, uint32_t Y)
{
    TrackPopupMenu(MenuHandle, TPM_LEFTALIGN, X, Y, 0, CallbackWnd, nullptr);
}

void UHMenuGUI::SetOptionActive(int32_t Index, bool bActive)
{
    EnableMenuItem(MenuHandle, Index, (bActive) ? MF_ENABLED | MF_BYPOSITION : MF_GRAYED | MF_BYPOSITION);
}

HMENU UHMenuGUI::GetMenu() const
{
	return MenuHandle;
}

#endif