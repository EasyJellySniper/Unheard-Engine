#include "Dialog.h"

std::unordered_map<int32_t, UHDialog*> GActiveDialogTable;
std::unordered_map<HWND, UHGUIControlBase*> GControlGUITable;

UHDialog::UHDialog(HINSTANCE InInstance, HWND InWindow)
	: Instance(InInstance)
	, ParentWindow(InWindow)
    , Dialog(nullptr)
    , bIsOpened(false)
{

}

HWND UHDialog::GetDialog() const
{
    return Dialog;
}

INT_PTR CALLBACK GDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    HWND CtrlHwnd = (HWND)lParam;
    UHGUIControlBase* GUIControl = nullptr;
    if (GControlGUITable.find(CtrlHwnd) != GControlGUITable.end())
    {
        GUIControl = GControlGUITable[CtrlHwnd];
    }

    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDCANCEL)
        {
            for (auto It = GActiveDialogTable.begin(); It != GActiveDialogTable.end(); )
            {
                if (It->second->GetDialog() == hDlg)
                {
                    for (auto& Callback : It->second->OnDestroy)
                    {
                        Callback();
                    }
                    GActiveDialogTable.erase(It);
                    break;
                }
                It++;
            }

            GActiveDialogTable.erase(GetDlgCtrlID(hDlg));
            EndDialog(hDlg, LOWORD(wParam));
        }
        else if (HIWORD(wParam) == EN_CHANGE)
        {
            // text box change
            if (GUIControl)
            {
                if (!GUIControl->IsSetFromCode())
                {
                    for (auto& Callback : GUIControl->OnEditText)
                    {
                        Callback();
                    }

#if WITH_EDITOR
                    for (auto& Callback : GUIControl->OnEditProperty)
                    {
                        Callback("");
                    }
#endif
                }
                GUIControl->SetIsFromCode(false);
            }
        }
        else if (HIWORD(wParam) == BN_CLICKED)
        {
            // button clicked
            if (GUIControl)
            {
                for (auto& Callback : GUIControl->OnClicked)
                {
                    Callback();
                }

#if WITH_EDITOR
                for (auto& Callback : GUIControl->OnEditProperty)
                {
                    Callback("");
                }
#endif
            }

            // it's also possible to hit a menu, send the low word to callback
            for (auto It = GActiveDialogTable.begin(); It != GActiveDialogTable.end(); )
            {
                if (It->second->GetDialog() == hDlg)
                {
                    for (auto& Callback : It->second->OnMenuClicked)
                    {
                        Callback(LOWORD(wParam));
                    }
                    break;
                }
                It++;
            }
        }
        else if (HIWORD(wParam) == CBN_SELCHANGE)
        {
            // combobox selected
            if (GUIControl)
            {
                for (auto& Callback : GUIControl->OnSelected)
                {
                    Callback();
                }

#if WITH_EDITOR
                for (auto& Callback : GUIControl->OnEditProperty)
                {
                    Callback("");
                }
#endif
            }
        }
        else if (HIWORD(wParam) == LBN_DBLCLK)
        {
            // double click on list box
            if (GUIControl && (HWND)lParam == GUIControl->GetHwnd())
            {
                for (auto& Callback : GUIControl->OnDoubleClicked)
                {
                    Callback();
                }
            }
        }
        else if (HIWORD(wParam) == LBN_SELCHANGE)
        {
            // select on list box
            if (GUIControl && (HWND)lParam == GUIControl->GetHwnd())
            {
                for (auto& Callback : GUIControl->OnSelected)
                {
                    Callback();
                }
            }
        }
        return (INT_PTR)TRUE;

     case WM_SIZE:
        // dialog resize, this needs to iterate all controls
        for (auto& GUI : GControlGUITable)
        {
            if (hDlg == GUI.second->GetParentHwnd())
            {
                int32_t Width = static_cast<int32_t>(LOWORD(lParam));
                int32_t Height = static_cast<int32_t>(HIWORD(lParam));
                GUI.second->Resize(Width, Height);
                
                // resize callback of the control
                for (auto& Callback : GUI.second->OnResized)
                {
                    Callback();
                }
            }
        }

        // resize dialog itself as well
        for (auto It = GActiveDialogTable.begin(); It != GActiveDialogTable.end(); )
        {
            if (It->second->GetDialog() == hDlg)
            {
                for (auto& Callback : It->second->OnResized)
                {
                    Callback();
                }
                break;
            }
            It++;
        }
        return (INT_PTR)TRUE;

     case WM_MOVE:
         for (auto It = GActiveDialogTable.begin(); It != GActiveDialogTable.end(); )
         {
             if (It->second->GetDialog() == hDlg)
             {
                 for (auto& Callback : It->second->OnMoved)
                 {
                     Callback();
                 }
                 break;
             }
             It++;
         }
         return (INT_PTR)TRUE;

    case WM_LBUTTONDOWN:
        SetCapture(hDlg);
        for (auto It = GActiveDialogTable.begin(); It != GActiveDialogTable.end(); )
        {
            if (It->second->GetDialog() == hDlg)
            {
                It->second->RawInput.SetLeftMousePressed(true);
                break;
            }
            It++;
        }
        return (INT_PTR)TRUE;

    case WM_RBUTTONDOWN:
        SetCapture(hDlg);
        for (auto It = GActiveDialogTable.begin(); It != GActiveDialogTable.end(); )
        {
            if (It->second->GetDialog() == hDlg)
            {
                It->second->RawInput.SetRightMousePressed(true);
                break;
            }
            It++;
        }
        return (INT_PTR)TRUE;

    case WM_LBUTTONUP:
        for (auto It = GActiveDialogTable.begin(); It != GActiveDialogTable.end(); )
        {
            if (It->second->GetDialog() == hDlg)
            {
                It->second->RawInput.SetLeftMousePressed(false);
                break;
            }
            It++;
        }
        ReleaseCapture();
        return (INT_PTR)TRUE;

    case WM_RBUTTONUP:
        for (auto It = GActiveDialogTable.begin(); It != GActiveDialogTable.end(); )
        {
            if (It->second->GetDialog() == hDlg)
            {
                It->second->RawInput.SetRightMousePressed(false);
                break;
            }
            It++;
        }
        ReleaseCapture();
        return (INT_PTR)TRUE;

    case WM_PAINT:
        PAINTSTRUCT Ps;
        HDC Hdc = BeginPaint(hDlg, &Ps);

        for (auto It = GActiveDialogTable.begin(); It != GActiveDialogTable.end(); )
        {
            if (It->second->GetDialog() == hDlg)
            {
                for (auto& Callback : It->second->OnPaint)
                {
                    Callback(Hdc);
                }
                break;
            }
            It++;
        }
        EndPaint(hDlg, &Ps);
        return (INT_PTR)TRUE;
    }

    return (INT_PTR)FALSE;
}