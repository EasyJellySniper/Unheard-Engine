#include "TextureCreationDialog.h"

#if WITH_DEBUG
#include "../../resource.h"
#include "../Classes/EditorUtils.h"
#include "../../Runtime/Classes/Utility.h"
#include <filesystem>
#include "TextureDialog.h"
#include "../../Runtime/Classes/AssetPath.h"

HWND GTextureCreationWindow = nullptr;
WPARAM GLatestTextureCreationControl = 0;

UHTextureCreationDialog::UHTextureCreationDialog()
    : UHDialog(nullptr, nullptr)
    , TextureDialog(nullptr)
    , TextureImporter(nullptr)
{

}

UHTextureCreationDialog::UHTextureCreationDialog(HINSTANCE InInstance, HWND InWindow, UHGraphic* InGfx, UHTextureDialog* InTextureDialog, UHTextureImporter* InTextureImporter)
	: UHDialog(InInstance, InWindow)
{
    TextureImporter = InTextureImporter;
    TextureDialog = InTextureDialog;

    ControlCallbacks[IDC_CREATETEXTURE] = { &UHTextureCreationDialog::ControlTextureCreate };
    ControlCallbacks[IDC_BROWSE_INPUT] = { &UHTextureCreationDialog::ControlBrowserInput };
    ControlCallbacks[IDC_BROWSE_OUTPUT] = { &UHTextureCreationDialog::ControlBrowserOutputFolder };
}

// Message handler for dialog
INT_PTR CALLBACK TextureCreationDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        GLatestTextureCreationControl = wParam;
        if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            GTextureCreationWindow = nullptr;
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

void UHTextureCreationDialog::ShowDialog()
{
    if (GTextureCreationWindow == nullptr)
    {
        // init texture creation dialog
        GTextureCreationWindow = CreateDialog(Instance, MAKEINTRESOURCE(IDD_TEXTURECREATE), Window, (DLGPROC)TextureCreationDialogProc);
        UHEditorUtil::InitComboBox(GTextureCreationWindow, IDC_TEXTURE_COMPRESSIONMODE, L"None", GCompressionModeText);

        ShowWindow(GTextureCreationWindow, SW_SHOW);
    }
}

void UHTextureCreationDialog::Update()
{
    if (GTextureCreationWindow == nullptr)
    {
        return;
    }

    // process control callbacks
    if (GLatestTextureCreationControl != 0)
    {
        if (ControlCallbacks.find(LOWORD(GLatestTextureCreationControl)) != ControlCallbacks.end())
        {
            if (HIWORD(GLatestTextureCreationControl) == EN_CHANGE || HIWORD(GLatestTextureCreationControl) == BN_CLICKED
                || HIWORD(GLatestTextureCreationControl) == CBN_SELCHANGE)
            {
                (this->*ControlCallbacks[LOWORD(GLatestTextureCreationControl)])();
            }
        }

        GLatestTextureCreationControl = 0;
    }
}

void UHTextureCreationDialog::TerminateDialog()
{
    if (GTextureCreationWindow)
    {
        EndDialog(GTextureCreationWindow, 0);
        GTextureCreationWindow = nullptr;
    }
}

void UHTextureCreationDialog::ControlTextureCreate()
{
    const std::filesystem::path InputSource = UHEditorUtil::GetEditControlText(GetDlgItem(GTextureCreationWindow, IDC_TEXTURE_FILE_1));
    std::filesystem::path OutputFolder = UHEditorUtil::GetEditControlText(GetDlgItem(GTextureCreationWindow, IDC_TEXTURE_FILE_OUTPUT));
    std::filesystem::path TextureAssetPath = GTextureAssetFolder;
    TextureAssetPath = std::filesystem::absolute(TextureAssetPath);
    const bool bIsValidOutputFolder = UHUtilities::StringFind(OutputFolder.string() + "\\", TextureAssetPath.string());

    if (!std::filesystem::exists(InputSource))
    {
        MessageBoxA(GTextureCreationWindow, "Invalid input source!", "Texture Creation", MB_OK);
        return;
    }

    if (!std::filesystem::exists(OutputFolder) || !bIsValidOutputFolder)
    {
        MessageBoxA(GTextureCreationWindow, "Invalid output folder or it's not under the engine path Assets/Textures!", "Texture Creation", MB_OK);
        return;
    }

    // remove the project folder
    OutputFolder = std::filesystem::relative(OutputFolder);

    UHTextureSettings TextureSetting;
    TextureSetting.bIsLinear = !UHEditorUtil::GetCheckedBox(GTextureCreationWindow, IDC_SRGB);
    TextureSetting.bIsNormal = UHEditorUtil::GetCheckedBox(GTextureCreationWindow, IDC_ISNORMAL);
    TextureSetting.CompressionSetting = (UHTextureCompressionSettings)UHEditorUtil::GetComboBoxSelectedIndex(GTextureCreationWindow, IDC_TEXTURE_COMPRESSIONMODE);

    std::filesystem::path RawSourcePath = std::filesystem::relative(InputSource);
    if (RawSourcePath.string().empty())
    {
        RawSourcePath = InputSource;
    }

    UHTexture* NewTex = TextureImporter->ImportRawTexture(RawSourcePath, OutputFolder, TextureSetting);
    TextureDialog->OnCreationFinished(NewTex);
}

void UHTextureCreationDialog::ControlBrowserInput()
{
    UHEditorUtil::SetEditControl(GTextureCreationWindow, IDC_TEXTURE_FILE_1, UHEditorUtil::FileSelectInput(GImageFilter));
}

void UHTextureCreationDialog::ControlBrowserOutputFolder()
{
    UHEditorUtil::SetEditControl(GTextureCreationWindow, IDC_TEXTURE_FILE_OUTPUT, UHEditorUtil::FileSelectOutputFolder());
}

#endif