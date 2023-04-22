#include "EditorUtils.h"

#if WITH_DEBUG
#include "../../Runtime/Classes/Utility.h"
#include <CommCtrl.h>

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

    void SetEditControl(HWND Hwnd, std::wstring InValue)
    {
        Edit_SetText(Hwnd, InValue.c_str());
    }

    void SetEditControlChar(HWND Hwnd, int32_t ControlID, char InValue)
    {
        std::string Value;
        Value += InValue;
        SetDlgItemTextA(Hwnd, ControlID, Value.c_str());
    }

    void InitComboBox(HWND Hwnd, int32_t BoxID, std::wstring DefaultValue, std::vector<std::wstring> Options, int32_t MinVisible)
    {
        HWND ComboBox = GetDlgItem(Hwnd, BoxID);

        for (const std::wstring& Option : Options)
        {
            ComboBox_AddString(ComboBox, Option.c_str());
        }
        ComboBox_SelectString(ComboBox, 0, DefaultValue.c_str());

        if (MinVisible > 0)
        {
            ComboBox_SetMinVisible(ComboBox, MinVisible);
        }
    }

    void InitComboBox(HWND Hwnd, std::string DefaultValue, std::vector<std::string> Options, int32_t MinVisible)
    {
        HWND ComboBox = Hwnd;

        for (const std::string& Option : Options)
        {
            ComboBox_AddString(ComboBox, UHUtilities::ToStringW(Option).c_str());
        }
        ComboBox_SelectString(ComboBox, 0, UHUtilities::ToStringW(DefaultValue).c_str());

        if (MinVisible > 0)
        {
            ComboBox_SetMinVisible(ComboBox, MinVisible);
        }
    }

    void SelectComboBox(HWND Hwnd, std::string Value)
    {
        ComboBox_SelectString(Hwnd, 0, UHUtilities::ToStringW(Value).c_str());
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

    std::string GetEditControlText(HWND Hwnd)
    {
        wchar_t Buff[1024];
        memset(Buff, 0, 1024);
        Edit_GetText(Hwnd, Buff, 1024);
        return UHUtilities::ToStringA(Buff);
    }

    int32_t GetComboBoxSelectedIndex(HWND Hwnd, int32_t BoxID)
    {
        HWND ComboBox = GetDlgItem(Hwnd, BoxID);
        return ComboBox_GetCurSel(ComboBox);
    }

    int32_t GetComboBoxSelectedIndex(HWND Hwnd)
    {
        return ComboBox_GetCurSel(Hwnd);
    }

    std::string GetComboBoxSelectedText(HWND Hwnd)
    {
        wchar_t Buff[1024];
        memset(Buff, 0, 1024);
        Buff[1023] = L'\0';
        ComboBox_GetText(Hwnd, Buff, 1024);

        return UHUtilities::ToStringA(Buff);
    }

    void AddListBoxString(HWND HWnd, int32_t BoxID, std::wstring InValue)
    {
        // get list box and add the string
        HWND ListBox = GetDlgItem(HWnd, BoxID);
        SendMessage(ListBox, LB_ADDSTRING, 0, (LPARAM)InValue.c_str());
    }

    int32_t GetListBoxSelectedIndex(HWND HWnd, int32_t BoxID)
    {
        HWND ListBox = GetDlgItem(HWnd, BoxID);
        return static_cast<int32_t>(SendMessage(ListBox, LB_GETCURSEL, 0, 0));
    }

    void SetWindowSize(HWND Hwnd, uint32_t X, uint32_t Y, uint32_t Width, uint32_t Height)
    {
        // keep previous window position, this function is for setting size only
        MoveWindow(Hwnd, X, Y, Width, Height, true);
    }

    void GetWindowSize(HWND Hwnd, RECT& OutRect, HWND ParentWnd)
    {
        RECT Rect;
        if (GetWindowRect(Hwnd, &Rect))
        {
            OutRect = Rect;

            // get relative pos
            MapWindowPoints(HWND_DESKTOP, (ParentWnd != nullptr) ? ParentWnd : Hwnd, (LPPOINT)&OutRect, 2);
        }
    }

    bool IsPointInsideClient(HWND Hwnd, POINT P)
    {
        if (ScreenToClient(Hwnd, &P))
        {
            RECT R;
            GetClientRect(Hwnd, &R);
            if (R.left < P.x && R.right > P.x && R.top < P.y && R.bottom > P.y)
            {
                return true;
            }
        }

        return false;
    }
}

#endif