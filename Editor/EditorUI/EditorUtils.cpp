#include "EditorUtils.h"

#if WITH_DEBUG

namespace UHEditorUtil
{
	void SetMenuItemChecked(HWND Hwnd, int32_t MenuItemID, UINT Flag)
	{
        HMENU Menu = GetMenu(Hwnd);
        MENUITEMINFO MenuItemInfo{};
        MenuItemInfo.cbSize = sizeof(MENUITEMINFO);
        MenuItemInfo.fMask = MIIM_STATE;

        MenuItemInfo.fState = Flag;
        SetMenuItemInfo(Menu, MenuItemID, FALSE, &MenuItemInfo);
	}

    void SetCheckedBox(HWND Hwnd, int32_t BoxID, bool bFlag)
    {
        CheckDlgButton(Hwnd, BoxID, bFlag ? BST_CHECKED : BST_UNCHECKED);
    }

    void SetEditControl(HWND Hwnd, int32_t ControlID, std::wstring InValue)
    {
        SetDlgItemText(Hwnd, ControlID, InValue.c_str());
    }

    void SetEditControlChar(HWND Hwnd, int32_t ControlID, char InValue)
    {
        std::string Value;
        Value += InValue;
        SetDlgItemTextA(Hwnd, ControlID, Value.c_str());
    }

    void InitComboBox(HWND Hwnd, int32_t BoxID, std::wstring DefaultValue, std::vector<std::wstring> Options)
    {
        HWND ComboBox = GetDlgItem(Hwnd, BoxID);

        for (const std::wstring Option : Options)
        {
            ComboBox_AddString(ComboBox, Option.c_str());
        }
        ComboBox_SelectString(ComboBox, 0, DefaultValue.c_str());
    }

    bool GetCheckedBox(HWND Hwnd, int32_t BoxID)
    {
        return IsDlgButtonChecked(Hwnd, BoxID);
    }

    char GetEditControlChar(HWND Hwnd, int32_t ControlID)
    {
        char Buff[1024];
        memset(Buff, 0, 1024);
        GetDlgItemTextA(Hwnd, ControlID, Buff, 1024);

        return Buff[0];
    }

    int32_t GetComboBoxSelectedIndex(HWND Hwnd, int32_t BoxID)
    {
        HWND ComboBox = GetDlgItem(Hwnd, BoxID);
        return ComboBox_GetCurSel(ComboBox);
    }
}

#endif