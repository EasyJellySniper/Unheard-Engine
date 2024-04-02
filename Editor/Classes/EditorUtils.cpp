#include "EditorUtils.h"
#include "../../../../Runtime/Classes/Utility.h"
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

    std::wstring FileSelectInput(const COMDLG_FILTERSPEC& InFilter, const std::wstring InDefaultFolder)
    {
        std::wstring SelectedFile;
        IFileDialog* FileDialog;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&FileDialog))))
        {
            if (InDefaultFolder.size() > 0)
            {
                IShellItem* CurFolder = NULL;
                if (SUCCEEDED(SHCreateItemFromParsingName(InDefaultFolder.c_str(), NULL, IID_PPV_ARGS(&CurFolder))))
                {
                    FileDialog->SetFolder(CurFolder);
                    CurFolder->Release();
                }
            }
            FileDialog->SetFileTypes(1, &InFilter);

            if (SUCCEEDED(FileDialog->Show(GetActiveWindow())))
            {
                IShellItem* Result;
                if (SUCCEEDED(FileDialog->GetResult(&Result)))
                {
                    wchar_t* Path;
                    if (SUCCEEDED(Result->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &Path)))
                    {
                        SelectedFile = Path;
                    }
                    Result->Release();
                }
            }
            FileDialog->Release();
        }

        return SelectedFile;
    }

    std::wstring FileSelectOutputFolder(const std::wstring InDefaultFolder)
    {
        std::wstring OutputFolder;
        IFileDialog* FileDialog;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&FileDialog))))
        {
            if (InDefaultFolder.size() > 0)
            {
                IShellItem* CurFolder = NULL;
                if (SUCCEEDED(SHCreateItemFromParsingName(InDefaultFolder.c_str(), NULL, IID_PPV_ARGS(&CurFolder))))
                {
                    FileDialog->SetFolder(CurFolder);
                    CurFolder->Release();
                }
            }

            DWORD Options;
            if (SUCCEEDED(FileDialog->GetOptions(&Options)))
            {
                FileDialog->SetOptions(Options | FOS_PICKFOLDERS);
            }

            if (SUCCEEDED(FileDialog->Show(GetActiveWindow())))
            {
                IShellItem* Result;
                if (SUCCEEDED(FileDialog->GetResult(&Result)))
                {
                    wchar_t* Path;
                    if (SUCCEEDED(Result->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &Path)))
                    {
                        OutputFolder = Path;
                    }
                    Result->Release();
                }
            }
            FileDialog->Release();
        }

        return OutputFolder;
    }

    std::wstring FileSelectSavePath(const COMDLG_FILTERSPEC& InFilter, const std::wstring InDefaultFolder)
    {
        std::wstring OutputPath;
        IFileDialog* FileDialog;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&FileDialog))))
        {
            if (InDefaultFolder.size() > 0)
            {
                IShellItem* CurFolder = NULL;
                if (SUCCEEDED(SHCreateItemFromParsingName(InDefaultFolder.c_str(), NULL, IID_PPV_ARGS(&CurFolder))))
                {
                    FileDialog->SetFolder(CurFolder);
                    CurFolder->Release();
                }
            }
            FileDialog->SetFileTypes(1, &InFilter);

            DWORD Options;
            if (SUCCEEDED(FileDialog->GetOptions(&Options)))
            {
                FileDialog->SetOptions(Options | FOS_OVERWRITEPROMPT);
            }

            if (SUCCEEDED(FileDialog->Show(GetActiveWindow())))
            {
                IShellItem* Result;
                if (SUCCEEDED(FileDialog->GetResult(&Result)))
                {
                    wchar_t* Path;
                    if (SUCCEEDED(Result->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &Path)))
                    {
                        OutputPath = Path;
                    }
                    Result->Release();
                }
            }
            FileDialog->Release();
        }

        return OutputPath;
    }

    SIZE GetTextSize(HWND Hwnd, std::string InText)
    {
        HDC DC = GetDC(Hwnd);
        SIZE TextSize;
        GetTextExtentPoint32A(DC, InText.c_str(), static_cast<int32_t>(InText.length()), &TextSize);
        ReleaseDC(Hwnd, DC);

        return TextSize;
    }

    SIZE GetTextSizeW(HWND Hwnd, std::wstring InText)
    {
        HDC DC = GetDC(Hwnd);
        SIZE TextSize;
        GetTextExtentPoint32(DC, InText.c_str(), static_cast<int32_t>(InText.length()), &TextSize);
        ReleaseDC(Hwnd, DC);

        return TextSize;
    }
}