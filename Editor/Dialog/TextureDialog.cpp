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
#include "StatusDialog.h"

// texture creation
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
}

UHTextureDialog::~UHTextureDialog()
{
    UH_SAFE_RELEASE(PreviewScene);
    PreviewScene.reset();
}

void UHTextureDialog::ShowDialog()
{
    if (!IsDialogActive(IDD_TEXTURE))
    {
        Init();
    }
}

void UHTextureDialog::Init()
{
    Dialog = CreateDialog(Instance, MAKEINTRESOURCE(IDD_TEXTURE), ParentWindow, (DLGPROC)GDialogProc);
    RegisterUniqueActiveDialog(IDD_TEXTURE, this);
    TextureCreationDialog = UHTextureCreationDialog(Instance, Dialog, Gfx, this, &TextureImporter);

    // init GUI controls
    const std::vector<UHTexture2D*>& Textures = AssetMgr->GetTexture2Ds();
    TextureListGUI = MakeUnique<UHListBox>(GetDlgItem(Dialog, IDC_TEXTURE_LIST), UHGUIProperty().SetAutoSize(AutoSizeY));
    for (const UHTexture2D* Tex : Textures)
    {
        TextureListGUI->AddItem(Tex->GetSourcePath());
    }

    CreateButtonGUI = MakeUnique<UHButton>(GetDlgItem(Dialog, IDC_TEXTURE_CREATE), UHGUIProperty().SetAutoMove(AutoMoveY));
    CreateButtonGUI->OnClicked.push_back(StdBind(&UHTextureDialog::ControlTextureCreate, this));

    SaveButtonGUI = MakeUnique<UHButton>(GetDlgItem(Dialog, IDC_TEXTURE_SAVE), UHGUIProperty().SetAutoMove(AutoMoveY));
    SaveButtonGUI->OnClicked.push_back(StdBind(&UHTextureDialog::ControlSave, this));

    SaveAllButtonGUI = MakeUnique<UHButton>(GetDlgItem(Dialog, IDC_TEXTURE_SAVEALL), UHGUIProperty().SetAutoMove(AutoMoveY));
    SaveAllButtonGUI->OnClicked.push_back(StdBind(&UHTextureDialog::ControlSaveAll, this));

    SizeLabelGUI = MakeUnique<UHLabel>(GetDlgItem(Dialog, IDC_TEXTURE_DIMENSION));
    MipLabelGUI = MakeUnique<UHLabel>(GetDlgItem(Dialog, IDC_MIPTEXT));
    MipSliderGUI = MakeUnique<UHSlider>(GetDlgItem(Dialog, IDC_MIPLEVEL_SLIDER));

    CompressionGUI = MakeUnique<UHComboBox>(GetDlgItem(Dialog, IDC_TEXTURE_COMPRESSIONMODE), UHGUIProperty().SetAutoMove(AutoMoveX));
    CompressionGUI->Init(L"None", GCompressionModeText);

    SrgbGUI = MakeUnique<UHCheckBox>(GetDlgItem(Dialog, IDC_SRGB), UHGUIProperty().SetAutoMove(AutoMoveX));
    NormalGUI = MakeUnique<UHCheckBox>(GetDlgItem(Dialog, IDC_ISNORMAL), UHGUIProperty().SetAutoMove(AutoMoveX));

    ApplyGUI = MakeUnique<UHButton>(GetDlgItem(Dialog, IDC_TEXTURE_APPLY), UHGUIProperty().SetAutoMove(AutoMoveX));
    ApplyGUI->OnClicked.push_back(StdBind(&UHTextureDialog::ControlApply, this));

    FileNameGUI = MakeUnique<UHTextBox>(GetDlgItem(Dialog, IDC_TEXTURE_RAWSOURCEPATH), UHGUIProperty().SetAutoMove(AutoMoveX));
    BrowseGUI = MakeUnique<UHButton>(GetDlgItem(Dialog, IDC_BROWSE_RAWTEXTURE), UHGUIProperty().SetAutoMove(AutoMoveX));
    BrowseGUI->OnClicked.push_back(StdBind(&UHTextureDialog::ControlBrowseRawTexture, this));

    CompressionTextGUI = MakeUnique<UHLabel>(GetDlgItem(Dialog, IDC_COMPRESSMODE_TEXT), UHGUIProperty().SetAutoMove(AutoMoveX));
    SourceFileTextGUI = MakeUnique<UHLabel>(GetDlgItem(Dialog, IDC_SOURCEFILE_TEXT), UHGUIProperty().SetAutoMove(AutoMoveX));
    HintTextGUI = MakeUnique<UHLabel>(GetDlgItem(Dialog, IDC_HINT_TEXT), UHGUIProperty().SetAutoMove(AutoMoveX));

    // reset the preview scene
    {
        TexturePreviewGUI = MakeUnique<UHGroupBox>(GetDlgItem(Dialog, IDC_TEXTUREPREVIEW), UHGUIProperty().SetAutoSize(AutoSizeBoth));
        TexturePreviewGUI->OnResized.push_back(StdBind(&UHTextureDialog::ControlTexturePreview, this));
        UH_SAFE_RELEASE(PreviewScene);
        PreviewScene = MakeUnique<UHPreviewScene>(Instance, TexturePreviewGUI->GetHwnd(), Gfx, TexturePreview);
    }

    CurrentTextureIndex = -1;
    CurrentTexture = nullptr;

    ShowWindow(Dialog, SW_SHOW);
}

void UHTextureDialog::Update()
{
    if (!IsDialogActive(IDD_TEXTURE))
    {
        return;
    }

    // get current selected texture index
    const int32_t TexIndex = TextureListGUI->GetSelectedIndex();
    if (TexIndex != CurrentTextureIndex)
    {
        // select texture
        SelectTexture(TexIndex);
        CurrentTextureIndex = TexIndex;
    }

    UpdatePreviewScene();
}

void UHTextureDialog::OnCreationFinished(UHTexture* InTexture)
{
    if (!UHUtilities::FindByElement(AssetMgr->GetTexture2Ds(), dynamic_cast<UHTexture2D*>(InTexture)))
    {
        TextureListGUI->AddItem(InTexture->GetSourcePath());
        AssetMgr->AddTexture2D(dynamic_cast<UHTexture2D*>(InTexture));
    }

    // force selecting the created texture
    const int32_t NewIdx = UHUtilities::FindIndex(AssetMgr->GetTexture2Ds(), static_cast<UHTexture2D*>(InTexture));
    TextureListGUI->Select(NewIdx);
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
    std::wstring SizeText;
    VkExtent2D TexExtent = CurrentTexture->GetExtent();
    SizeText = L"Dimension: " + std::to_wstring(TexExtent.width) + L" x " + std::to_wstring(TexExtent.height);
    SizeText += L" (" + std::to_wstring(CurrentTexture->GetTextureData().size() / 1024) + L" KB)";
    SizeLabelGUI->SetText(SizeText);

    uint32_t MipCount = CurrentTexture->GetMipMapCount();
    HWND MipSlider = GetDlgItem(Dialog, IDC_MIPLEVEL_SLIDER);

    CurrentMip = 0;
    MipLabelGUI->SetText(L"Mip: " + std::to_wstring(CurrentMip));
    MipSliderGUI->Range(0, MipCount - 1).SetPos(CurrentMip);

    // sync the sRGB / is normal flag
    const UHTextureSettings& Setting = CurrentTexture->GetTextureSettings();
    SrgbGUI->Checked(!Setting.bIsLinear);
    NormalGUI->Checked(Setting.bIsNormal);

    // sync compression setting
    CompressionGUI->Select(GCompressionModeText[Setting.CompressionSetting]);

    // sync raw source path
    FileNameGUI->SetText(CurrentTexture->GetRawSourcePath());

    PreviewScene->SetPreviewTexture(CurrentTexture);
    PreviewScene->SetPreviewMip(CurrentMip);
}

void UHTextureDialog::UpdatePreviewScene()
{
    uint32_t DesiredMip = MipSliderGUI->GetPos();
    if (DesiredMip != CurrentMip)
    {
        CurrentMip = DesiredMip;
        PreviewScene->SetPreviewMip(CurrentMip);
        MipLabelGUI->SetText("Mip: " + std::to_string(CurrentMip));
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

    HWND CurrDialog = Dialog;
    UHStatusDialogScope StatusDialog(Instance, CurrDialog, "Processing...");

    std::string RawSource = UHUtilities::ToStringA(FileNameGUI->GetText());
    std::filesystem::path RawSourcePath(RawSource);
    if (!std::filesystem::exists(RawSourcePath))
    {
        MessageBoxA(CurrDialog, "Please select a valid image source file and try again.", "Missing source file", MB_OK);
        return;
    }
    CurrentTexture->SetRawSourcePath(RawSourcePath.string());

    // recreate texture if any option is changed
    const bool bIsLinear = !SrgbGUI->IsChecked();
    const bool bIsNormal = NormalGUI->IsChecked();
    const UHTextureSettings OldSetting = CurrentTexture->GetTextureSettings();

    UHTextureSettings NewSetting;
    NewSetting.bIsLinear = bIsLinear;
    NewSetting.bIsNormal = bIsNormal;
    NewSetting.CompressionSetting = (UHTextureCompressionSettings)CompressionGUI->GetSelectedIndex();
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
    MessageBoxA(Dialog, "Current editing texture is saved.\nIt's also recommended to resave referencing materials.", "Texture Editor", MB_OK);
}

void UHTextureDialog::ControlSaveAll()
{
    // iterate all textures and save them
    for (UHTexture2D* Tex : AssetMgr->GetTexture2Ds())
    {
        const std::filesystem::path SourcePath = Tex->GetSourcePath();
        Tex->Export(GTextureAssetFolder + SourcePath.string());
    }
    MessageBoxA(Dialog, "All textures are saved.", "Texture Editor", MB_OK);
}

void UHTextureDialog::ControlTextureCreate()
{
    TextureCreationDialog.ShowDialog();
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

    FileNameGUI->SetText(RawSourcePath.wstring());
}

void UHTextureDialog::ControlTexturePreview()
{
    // reset preview scene
    Gfx->WaitGPU();
    UH_SAFE_RELEASE(PreviewScene);
    PreviewScene = MakeUnique<UHPreviewScene>(Instance, TexturePreviewGUI->GetHwnd(), Gfx, TexturePreview);
    PreviewScene->SetPreviewTexture(CurrentTexture);
    PreviewScene->SetPreviewMip(CurrentMip);
}

#endif