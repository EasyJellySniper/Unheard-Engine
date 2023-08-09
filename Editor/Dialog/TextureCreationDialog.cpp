#include "TextureCreationDialog.h"

#if WITH_DEBUG
#include "../../resource.h"
#include "../Classes/EditorUtils.h"
#include "../../Runtime/Classes/Utility.h"
#include <filesystem>
#include "TextureDialog.h"
#include "../../Runtime/Classes/AssetPath.h"
#include "StatusDialog.h"

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
}

void UHTextureCreationDialog::ShowDialog()
{
    if (!IsDialogActive(IDD_TEXTURECREATE))
    {
        // init texture creation dialog
        Dialog = CreateDialog(Instance, MAKEINTRESOURCE(IDD_TEXTURECREATE), ParentWindow, (DLGPROC)GDialogProc);
        RegisterUniqueActiveDialog(IDD_TEXTURECREATE, this);

        CompressionModeGUI = MakeUnique<UHComboBox>(GetDlgItem(Dialog, IDC_TEXTURE_COMPRESSIONMODE));
        CompressionModeGUI->Init(L"None", GCompressionModeText);

        SrgbGUI = MakeUnique<UHCheckBox>(GetDlgItem(Dialog, IDC_SRGB));
        NormalGUI = MakeUnique<UHCheckBox>(GetDlgItem(Dialog, IDC_ISNORMAL));

        CreateTextureGUI = MakeUnique<UHButton>(GetDlgItem(Dialog, IDC_CREATETEXTURE));
        CreateTextureGUI->OnClicked.push_back(StdBind(&UHTextureCreationDialog::ControlTextureCreate, this));

        BrowseInputGUI = MakeUnique<UHButton>(GetDlgItem(Dialog, IDC_BROWSE_INPUT));
        BrowseInputGUI->OnClicked.push_back(StdBind(&UHTextureCreationDialog::ControlBrowserInput, this));

        BrowseOutputGUI = MakeUnique<UHButton>(GetDlgItem(Dialog, IDC_BROWSE_OUTPUT));
        BrowseOutputGUI->OnClicked.push_back(StdBind(&UHTextureCreationDialog::ControlBrowserOutputFolder, this));

        TextureInputGUI = MakeUnique<UHTextBox>(GetDlgItem(Dialog, IDC_TEXTURE_FILE_1));
        TextureOutputGUI = MakeUnique<UHTextBox>(GetDlgItem(Dialog, IDC_TEXTURE_FILE_OUTPUT));

        ShowWindow(Dialog, SW_SHOW);
    }
}

void UHTextureCreationDialog::ControlTextureCreate()
{
    HWND CurrDialog = Dialog;

    const std::filesystem::path InputSource = TextureInputGUI->GetText();
    std::filesystem::path OutputFolder = TextureOutputGUI->GetText();
    std::filesystem::path TextureAssetPath = GTextureAssetFolder;
    TextureAssetPath = std::filesystem::absolute(TextureAssetPath);
    const bool bIsValidOutputFolder = UHUtilities::StringFind(OutputFolder.string() + "\\", TextureAssetPath.string());

    if (!std::filesystem::exists(InputSource))
    {
        MessageBoxA(CurrDialog, "Invalid input source!", "Texture Creation", MB_OK);
        return;
    }

    if (!std::filesystem::exists(OutputFolder) || !bIsValidOutputFolder)
    {
        MessageBoxA(CurrDialog, "Invalid output folder or it's not under the engine path Assets/Textures!", "Texture Creation", MB_OK);
        return;
    }

    UHStatusDialogScope StatusDialog(Instance, CurrDialog, "Creating...");

    // remove the project folder
    OutputFolder = std::filesystem::relative(OutputFolder);

    UHTextureSettings TextureSetting;
    TextureSetting.bIsLinear = !SrgbGUI->IsChecked();
    TextureSetting.bIsNormal = NormalGUI->IsChecked();
    TextureSetting.CompressionSetting = (UHTextureCompressionSettings)CompressionModeGUI->GetSelectedIndex();

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
    TextureInputGUI->SetText(UHEditorUtil::FileSelectInput(GImageFilter));
}

void UHTextureCreationDialog::ControlBrowserOutputFolder()
{
    TextureOutputGUI->SetText(UHEditorUtil::FileSelectOutputFolder());
}

#endif