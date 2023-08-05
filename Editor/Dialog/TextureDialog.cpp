#include "TextureDialog.h"

#if WITH_DEBUG

#include "../Classes/EditorUtils.h"
#include "../../resource.h"
#include "../../Runtime/Engine/Asset.h"
#include "../../Runtime/Classes/Texture2D.h"
#include "../Editor/PreviewScene.h"
#include "../../Runtime/Renderer/GraphicBuilder.h"
#include "../../Runtime/Renderer/DeferredShadingRenderer.h"
#include "../../Runtime/Classes/AssetPath.h"
#include <filesystem>

HWND GTextureWindow = nullptr;
WPARAM GLatestTextureControl = 0;

// texture creation
UHTextureCreationDialog GTextureCreationDialog;

UHTextureDialog::UHTextureDialog(HINSTANCE InInstance, HWND InWindow, UHAssetManager* InAssetMgr, UHGraphic* InGfx, UHDeferredShadingRenderer* InRenderer)
	: UHDialog(InInstance, InWindow)
	, AssetMgr(InAssetMgr)
    , Gfx(InGfx)
    , Renderer(InRenderer)
    , CurrentTexture(nullptr)
    , CurrentTextureIndex(-1)
    , CurrentMip(0)
{
    TextureImporter = UHTextureImporter(InGfx);

    // register callback functions
    ControlCallbacks[IDC_TEXTURE_APPLY] = { &UHTextureDialog::ControlApply };
    ControlCallbacks[IDC_TEXTURE_SAVE] = { &UHTextureDialog::ControlSave };
    ControlCallbacks[IDC_TEXTURE_SAVEALL] = { &UHTextureDialog::ControlSaveAll };
    ControlCallbacks[IDC_TEXTURE_CREATE] = { &UHTextureDialog::ControlTextureCreate };
    ControlCallbacks[IDC_BROWSE_RAWTEXTURE] = { &UHTextureDialog::ControlBrowseRawTexture };
}

UHTextureDialog::~UHTextureDialog()
{
    UH_SAFE_RELEASE(PreviewScene);
    PreviewScene.reset();
}

// Message handler for dialog
INT_PTR CALLBACK TextureDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        GLatestTextureControl = wParam;
        if (LOWORD(wParam) == IDCANCEL)
        {
            GTextureCreationDialog.TerminateDialog();
            EndDialog(hDlg, LOWORD(wParam));
            GTextureWindow = nullptr;
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

void UHTextureDialog::ShowDialog()
{
    if (GTextureWindow == nullptr)
    {
        Init();
        ShowWindow(GTextureWindow, SW_SHOW);
    }
}

void UHTextureDialog::Init()
{
    GTextureWindow = CreateDialog(Instance, MAKEINTRESOURCE(IDD_TEXTURE), Window, (DLGPROC)TextureDialogProc);
    GTextureCreationDialog = UHTextureCreationDialog(Instance, GTextureWindow, Gfx, this, &TextureImporter);

    // reset the preview scene
    {
        UH_SAFE_RELEASE(PreviewScene);
        PreviewScene.reset();
        HWND TexturePreviewArea = GetDlgItem(GTextureWindow, IDC_TEXTUREPREVIEW);
        PreviewScene = std::make_unique<UHPreviewScene>(Instance, TexturePreviewArea, Gfx, TexturePreview);
    }

    // get the texture list from assets manager
    const std::vector<UHTexture2D*>& Textures = AssetMgr->GetTexture2Ds();
    for (const UHTexture2D* Tex : Textures)
    {
        UHEditorUtil::AddListBoxString(GTextureWindow, IDC_TEXTURE_LIST, Tex->GetSourcePath());
    }

    // compression setting combobox
    UHEditorUtil::InitComboBox(GTextureWindow, IDC_TEXTURE_COMPRESSIONMODE, L"None", GCompressionModeText);

    CurrentTextureIndex = -1;
    CurrentTexture = nullptr;
    GLatestTextureControl = 0;
}

void UHTextureDialog::Update()
{
    if (GTextureWindow == nullptr)
    {
        return;
    }

    // process control callbacks
    if (GLatestTextureControl != 0)
    {
        if (ControlCallbacks.find(LOWORD(GLatestTextureControl)) != ControlCallbacks.end())
        {
            if (HIWORD(GLatestTextureControl) == EN_CHANGE || HIWORD(GLatestTextureControl) == BN_CLICKED
                || HIWORD(GLatestTextureControl) == CBN_SELCHANGE)
            {
                (this->*ControlCallbacks[LOWORD(GLatestTextureControl)])();
            }
        }

        GLatestTextureControl = 0;
    }

    // get current selected texture index
    const int32_t TexIndex = UHEditorUtil::GetListBoxSelectedIndex(GTextureWindow, IDC_TEXTURE_LIST);
    if (TexIndex != CurrentTextureIndex)
    {
        // select texture
        SelectTexture(TexIndex);
        CurrentTextureIndex = TexIndex;
    }

    UpdatePreviewScene();
    GTextureCreationDialog.Update();
}

void UHTextureDialog::OnCreationFinished(UHTexture* InTexture)
{
    if (!UHUtilities::FindByElement(AssetMgr->GetTexture2Ds(), dynamic_cast<UHTexture2D*>(InTexture)))
    {
        UHEditorUtil::AddListBoxString(GTextureWindow, IDC_TEXTURE_LIST, InTexture->GetSourcePath());
        AssetMgr->AddTexture2D(dynamic_cast<UHTexture2D*>(InTexture));
    }

    // force selecting the created texture
    const int32_t NewIdx = UHUtilities::FindIndex(AssetMgr->GetTexture2Ds(), static_cast<UHTexture2D*>(InTexture));
    UHEditorUtil::SetListBoxSelectedIndex(GetDlgItem(GTextureWindow, IDC_TEXTURE_LIST), NewIdx);
    SelectTexture(NewIdx);
    CurrentTextureIndex = NewIdx;
    Renderer->UpdateTextureDescriptors();
}

void UHTextureDialog::SelectTexture(int32_t TexIndex)
{
    CurrentTexture = AssetMgr->GetTexture2Ds()[TexIndex];
    if (!CurrentTexture->HasUploadedToGPU())
    {
        // upload to gpu if it's not in resident
        VkCommandBuffer UploadCmd = Gfx->BeginOneTimeCmd();
        UHGraphicBuilder UploadBuilder(Gfx, UploadCmd);
        CurrentTexture->UploadToGPU(Gfx, UploadCmd, UploadBuilder);
        Gfx->EndOneTimeCmd(UploadCmd);
    }

    // sync the dimension and mip level count to UI
    std::string SizeText;
    VkExtent2D TexExtent = CurrentTexture->GetExtent();
    SizeText = "Dimension: " + std::to_string(TexExtent.width) + " x " + std::to_string(TexExtent.height);
    SizeText += " (" + std::to_string(CurrentTexture->GetTextureData().size() / 1024) + " KB)";
    UHEditorUtil::SetStaticText(GetDlgItem(GTextureWindow, IDC_TEXTURE_DIMENSION), SizeText);

    uint32_t MipCount = CurrentTexture->GetMipMapCount();
    HWND MipSlider = GetDlgItem(GTextureWindow, IDC_MIPLEVEL_SLIDER);
    UHEditorUtil::SetSliderRange(MipSlider, 0, MipCount - 1);

    CurrentMip = 0;
    UHEditorUtil::SetStaticText(GetDlgItem(GTextureWindow, IDC_MIPTEXT), "Mip: " + std::to_string(CurrentMip));
    UHEditorUtil::SetSliderPos(MipSlider, CurrentMip);

    // sync the sRGB / is normal flag
    const UHTextureSettings& Setting = CurrentTexture->GetTextureSettings();
    UHEditorUtil::SetCheckedBox(GTextureWindow, IDC_SRGB, !Setting.bIsLinear);
    UHEditorUtil::SetCheckedBox(GTextureWindow, IDC_ISNORMAL, Setting.bIsNormal);

    // sync compression setting
    UHEditorUtil::SelectComboBox(GetDlgItem(GTextureWindow, IDC_TEXTURE_COMPRESSIONMODE), GCompressionModeText[Setting.CompressionSetting]);

    // sync raw source path
    UHEditorUtil::SetEditControl(GetDlgItem(GTextureWindow, IDC_TEXTURE_RAWSOURCEPATH), CurrentTexture->GetRawSourcePath());

    PreviewScene->SetPreviewTexture(CurrentTexture);
    PreviewScene->SetPreviewMip(CurrentMip);
}

void UHTextureDialog::UpdatePreviewScene()
{
    uint32_t DesiredMip = UHEditorUtil::GetSliderPos(GetDlgItem(GTextureWindow, IDC_MIPLEVEL_SLIDER));
    if (DesiredMip != CurrentMip)
    {
        CurrentMip = DesiredMip;
        PreviewScene->SetPreviewMip(CurrentMip);
        UHEditorUtil::SetStaticText(GetDlgItem(GTextureWindow, IDC_MIPTEXT), "Mip: " + std::to_string(CurrentMip));
    }

    if (CurrentTextureIndex != -1)
    {
        PreviewScene->Render();
    }
}

void UHTextureDialog::ControlApply()
{
    if (CurrentTexture == nullptr)
    {
        return;
    }

    std::string RawSource = UHEditorUtil::GetEditControlText(GetDlgItem(GTextureWindow, IDC_TEXTURE_RAWSOURCEPATH));
    std::filesystem::path RawSourcePath(RawSource);
    if (!std::filesystem::exists(RawSourcePath))
    {
        MessageBoxA(GTextureWindow, "Please select a valid image source file and try again.", "Missing source file", MB_OK);
        return;
    }
    CurrentTexture->SetRawSourcePath(RawSourcePath.string());

    // recreate texture if any option is changed
    const bool bIsLinear = !UHEditorUtil::GetCheckedBox(GTextureWindow, IDC_SRGB);
    const bool bIsNormal = UHEditorUtil::GetCheckedBox(GTextureWindow, IDC_ISNORMAL);
    const UHTextureSettings OldSetting = CurrentTexture->GetTextureSettings();

    UHTextureSettings NewSetting;
    NewSetting.bIsLinear = bIsLinear;
    NewSetting.bIsNormal = bIsNormal;
    NewSetting.CompressionSetting = (UHTextureCompressionSettings)UHEditorUtil::GetComboBoxSelectedIndex(GetDlgItem(GTextureWindow, IDC_TEXTURE_COMPRESSIONMODE));
    CurrentTexture->SetTextureSettings(NewSetting);

    // always recreating texture
    uint32_t W, H;
    std::vector<uint8_t> RawData = TextureImporter.LoadTexture(RawSourcePath.wstring(), W, H);
    CurrentTexture->SetExtent(W, H);
    CurrentTexture->SetTextureData(RawData);
    CurrentTexture->Recreate();
    Renderer->UpdateTextureDescriptors();

    const bool bIsNormalChanged = bIsNormal != OldSetting.bIsNormal;
    if (bIsNormalChanged)
    {
        // if normal is changed, all materials are referencing this texture needs a recompile
        for (UHObject* MatObj : CurrentTexture->GetReferenceObjects())
        {
            if (UHMaterial* Mat = static_cast<UHMaterial*>(MatObj))
            {
                Mat->SetCompileFlag(UHMaterialCompileFlag::FullCompileTemporary);
                Renderer->RefreshMaterialShaders(Mat);
            }
        }
    }

    SelectTexture(CurrentTextureIndex);
}

void UHTextureDialog::ControlSave()
{
    if (CurrentTexture == nullptr)
    {
        return;
    }

    const std::filesystem::path SourcePath = CurrentTexture->GetSourcePath();
    CurrentTexture->Export(GTextureAssetFolder + SourcePath.string());
    MessageBoxA(GTextureWindow, "Current editing texture is saved.\nIt's also recommended to resave referencing materials.", "Texture Editor", MB_OK);
}

void UHTextureDialog::ControlSaveAll()
{
    // iterate all textures and save them
    for (UHTexture2D* Tex : AssetMgr->GetTexture2Ds())
    {
        const std::filesystem::path SourcePath = Tex->GetSourcePath();
        Tex->Export(GTextureAssetFolder + SourcePath.string());
    }
    MessageBoxA(GTextureWindow, "All textures are saved.", "Texture Editor", MB_OK);
}

void UHTextureDialog::ControlTextureCreate()
{
    GTextureCreationDialog.ShowDialog();
}

void UHTextureDialog::ControlBrowseRawTexture()
{
    if (CurrentTexture == nullptr)
    {
        return;
    }

    std::filesystem::path FilePath = UHEditorUtil::FileSelectInput(GImageFilter);
    std::filesystem::path RawSourcePath = std::filesystem::relative(FilePath);
    if (RawSourcePath.string().empty())
    {
        // if the path is not relative to engine folder, just use the absolute path
        RawSourcePath = FilePath;
    }

    UHEditorUtil::SetEditControl(GTextureWindow, IDC_TEXTURE_RAWSOURCEPATH, RawSourcePath.wstring());
}

#endif