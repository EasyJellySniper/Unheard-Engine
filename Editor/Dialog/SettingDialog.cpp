#include "SettingDialog.h"

#if WITH_EDITOR
#include "../../Runtime/Engine/Config.h"
#include "../../Runtime/Engine/Engine.h"
#include "../../Runtime/Renderer/DeferredShadingRenderer.h"
#include "../../resource.h"

UHSettingDialog::UHSettingDialog()
	: UHSettingDialog(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)
{
}

UHSettingDialog::UHSettingDialog(HINSTANCE InInstance, HWND InWindow, UHConfigManager* InConfig, UHEngine* InEngine
    , UHDeferredShadingRenderer* InRenderer, UHRawInput* InRawInput)
	: UHDialog(InInstance, InWindow)
    , Config(InConfig)
    , Engine(InEngine)
    , DeferredRenderer(InRenderer)
    , Input(InRawInput)
{

}

void UHSettingDialog::ShowDialog()
{
    if (!IsDialogActive(IDD_SETTING))
    {
        Dialog = CreateDialog(Instance, MAKEINTRESOURCE(IDD_SETTING), ParentWindow, (DLGPROC)GDialogProc);
        RegisterUniqueActiveDialog(IDD_SETTING, this);

        // sync ini settings to the window controls
        const UHPresentationSettings& PresentSettings = Config->PresentationSetting();
        const UHEngineSettings& EngineSettings = Config->EngineSetting();
        const UHRenderingSettings& RenderingSettings = Config->RenderingSetting();

        // presentation
        VSyncGUI = MakeUnique<UHCheckBox>(GetDlgItem(Dialog, IDC_VSYNC));
        VSyncGUI->Checked(PresentSettings.bVsync);
        VSyncGUI->OnClicked.push_back(StdBind(&UHSettingDialog::ControlVsync, this));

        FullScreenGUI = MakeUnique<UHCheckBox>(GetDlgItem(Dialog, IDC_FULLSCREEN));
        FullScreenGUI->Checked(PresentSettings.bFullScreen);
        FullScreenGUI->OnClicked.push_back(StdBind(&UHSettingDialog::ControlFullScreen, this));

        // engine
        CameraMoveSpeedGUI = MakeUnique<UHTextBox>(GetDlgItem(Dialog, IDC_CAMERASPEED));
        CameraMoveSpeedGUI->Content((int32_t)EngineSettings.DefaultCameraMoveSpeed);
        CameraMoveSpeedGUI->OnEditText.push_back(StdBind(&UHSettingDialog::ControlCameraSpeed, this));

        MouseRotateSpeedGUI = MakeUnique<UHTextBox>(GetDlgItem(Dialog, IDC_MOUSEROTATESPEED));
        MouseRotateSpeedGUI->Content((int32_t)EngineSettings.MouseRotationSpeed);
        MouseRotateSpeedGUI->OnEditText.push_back(StdBind(&UHSettingDialog::ControlMouseRotateSpeed, this));

        ForwardKeyGUI = MakeUnique<UHTextBox>(GetDlgItem(Dialog, IDC_FORWARDKEY));
        ForwardKeyGUI->Content(EngineSettings.ForwardKey);
        ForwardKeyGUI->OnEditText.push_back(StdBind(&UHSettingDialog::ControlForwardKey, this));

        BackKeyGUI = MakeUnique<UHTextBox>(GetDlgItem(Dialog, IDC_BACKKEY));
        BackKeyGUI->Content(EngineSettings.BackKey);
        BackKeyGUI->OnEditText.push_back(StdBind(&UHSettingDialog::ControlBackKey, this));

        LeftKeyGUI = MakeUnique<UHTextBox>(GetDlgItem(Dialog, IDC_LEFTKEY));
        LeftKeyGUI->Content(EngineSettings.LeftKey);
        LeftKeyGUI->OnEditText.push_back(StdBind(&UHSettingDialog::ControlLeftKey, this));

        RightKeyGUI = MakeUnique<UHTextBox>(GetDlgItem(Dialog, IDC_RIGHTKEY));
        RightKeyGUI->Content(EngineSettings.RightKey);
        RightKeyGUI->OnEditText.push_back(StdBind(&UHSettingDialog::ControlRightKey, this));

        DownKeyGUI = MakeUnique<UHTextBox>(GetDlgItem(Dialog, IDC_DOWNKEY));
        DownKeyGUI->Content(EngineSettings.DownKey);
        DownKeyGUI->OnEditText.push_back(StdBind(&UHSettingDialog::ControlDownKey, this));

        UpKeyGUI = MakeUnique<UHTextBox>(GetDlgItem(Dialog, IDC_UPKEY));
        UpKeyGUI->Content(EngineSettings.UpKey);
        UpKeyGUI->OnEditText.push_back(StdBind(&UHSettingDialog::ControlUpKey, this));

        FPSLimitGUI = MakeUnique<UHTextBox>(GetDlgItem(Dialog, IDC_FPSLIMIT));
        FPSLimitGUI->Content((int32_t)EngineSettings.FPSLimit);
        FPSLimitGUI->OnEditText.push_back(StdBind(&UHSettingDialog::ControlFPSLimit, this));

        MeshMemBudgetGUI = MakeUnique<UHTextBox>(GetDlgItem(Dialog, IDC_MESHBUFFERMEMORYBUDGET));
        MeshMemBudgetGUI->Content((int32_t)EngineSettings.MeshBufferMemoryBudgetMB);
        MeshMemBudgetGUI->OnEditText.push_back(StdBind(&UHSettingDialog::ControlBufferMemoryBudget, this));

        TexMemBudgetGUI = MakeUnique<UHTextBox>(GetDlgItem(Dialog, IDC_IMAGEMEMORYBUDGET));
        TexMemBudgetGUI->Content((int32_t)EngineSettings.ImageMemoryBudgetMB);
        TexMemBudgetGUI->OnEditText.push_back(StdBind(&UHSettingDialog::ControlImageMemoryBudget, this));

        // rendering
        RenderWidthGUI = MakeUnique<UHTextBox>(GetDlgItem(Dialog, IDC_RENDERWIDTH));
        RenderWidthGUI->Content(RenderingSettings.RenderWidth);
        RenderHeightGUI = MakeUnique<UHTextBox>(GetDlgItem(Dialog, IDC_RENDERHEIGHT));
        RenderHeightGUI->Content(RenderingSettings.RenderHeight);

        ApplyResolutionGUI = MakeUnique<UHButton>(GetDlgItem(Dialog, IDC_APPLYRESOLUTION));
        ApplyResolutionGUI->OnClicked.push_back(StdBind(&UHSettingDialog::ControlResolution, this));

        EnableTaaGUI = MakeUnique<UHCheckBox>(GetDlgItem(Dialog, IDC_ENABLETAA));
        EnableTaaGUI->Checked(RenderingSettings.bTemporalAA);
        EnableTaaGUI->OnClicked.push_back(StdBind(&UHSettingDialog::ControlTAA, this));

        EnableRtGUI = MakeUnique<UHCheckBox>(GetDlgItem(Dialog, IDC_ENABLERAYTRACING));
        EnableRtGUI->Checked(RenderingSettings.bEnableRayTracing);
        EnableRtGUI->OnClicked.push_back(StdBind(&UHSettingDialog::ControlRayTracing, this));

        EnableGPULabelGUI = MakeUnique<UHCheckBox>(GetDlgItem(Dialog, IDC_ENABLEGPULABELING));
        EnableGPULabelGUI->Checked(RenderingSettings.bEnableGPULabeling);
        EnableGPULabelGUI->OnClicked.push_back(StdBind(&UHSettingDialog::ControlGPULabeling, this));

        EnableLayerValidationGUI = MakeUnique<UHCheckBox>(GetDlgItem(Dialog, IDC_ENABLELAYERVALIDATION));
        EnableLayerValidationGUI->Checked(RenderingSettings.bEnableLayerValidation);
        EnableLayerValidationGUI->OnClicked.push_back(StdBind(&UHSettingDialog::ControlLayerValidation, this));

        EnableGPUTimingGUI = MakeUnique<UHCheckBox>(GetDlgItem(Dialog, IDC_ENABLEGPUTIMING));
        EnableGPUTimingGUI->Checked(RenderingSettings.bEnableGPUTiming);
        EnableGPUTimingGUI->OnClicked.push_back(StdBind(&UHSettingDialog::ControlGPUTiming, this));

        EnableParallelSubmissionGUI = MakeUnique<UHCheckBox>(GetDlgItem(Dialog, IDC_ENABLEPARALLELSUBMISSION));
        EnableParallelSubmissionGUI->Checked(RenderingSettings.bParallelSubmission);
        EnableParallelSubmissionGUI->OnClicked.push_back(StdBind(&UHSettingDialog::ControlParallelSubmission, this));

        ParallelThreadCountGUI = MakeUnique<UHTextBox>(GetDlgItem(Dialog, IDC_PARALLELTHREADS));
        ParallelThreadCountGUI->Content(RenderingSettings.ParallelThreads);
        ParallelThreadCountGUI->OnEditText.push_back(StdBind(&UHSettingDialog::ControlParallelThread, this));

        EnableDepthPrePassGUI = MakeUnique<UHCheckBox>(GetDlgItem(Dialog, IDC_ENABLEDEPTHPREPASS));
        EnableDepthPrePassGUI->Checked(RenderingSettings.bEnableDepthPrePass);
        EnableDepthPrePassGUI->OnClicked.push_back(StdBind(&UHSettingDialog::ControlDepthPrePass, this));

        std::vector<std::wstring> ShadowQualities = { L"Full", L"Half", L"Quarter" };
        RTShadowQualityGUI = MakeUnique<UHComboBox>(GetDlgItem(Dialog, IDC_RTSHADOWQUALITY));
        RTShadowQualityGUI->Init(ShadowQualities[RenderingSettings.RTDirectionalShadowQuality], ShadowQualities);
        RTShadowQualityGUI->OnSelected.push_back(StdBind(&UHSettingDialog::ControlShadowQuality, this));

        EnableAsyncComputeGUI = MakeUnique<UHCheckBox>(GetDlgItem(Dialog, IDC_ENABLEASYNCCOMPUTE));
        EnableAsyncComputeGUI->Checked(RenderingSettings.bEnableAsyncCompute);
        EnableAsyncComputeGUI->OnClicked.push_back(StdBind(&UHSettingDialog::ControlAsyncCompute, this));

        ShowWindow(Dialog, SW_SHOW);
    }
}

void UHSettingDialog::Update()
{
    if (!IsDialogActive(IDD_SETTING))
    {
        return;
    }

    // sync settings from input event
    const UHPresentationSettings& PresentSettings = Config->PresentationSetting();
    const UHEngineSettings& EngineSettings = Config->EngineSetting();
    const UHRenderingSettings& RenderingSettings = Config->RenderingSetting();

    // full screen toggling
    if (Input->IsKeyHold(VK_MENU) && Input->IsKeyUp(VK_RETURN))
    {
        FullScreenGUI->Checked(!PresentSettings.bFullScreen);
    }

    // TAA toggling
    if (Input->IsKeyHold(VK_CONTROL) && Input->IsKeyUp('t'))
    {
        EnableTaaGUI->Checked(!RenderingSettings.bTemporalAA);
    }

    // vsync toggling
    if (Input->IsKeyHold(VK_CONTROL) && Input->IsKeyUp('v'))
    {
        VSyncGUI->Checked(!PresentSettings.bVsync);
    }
}

void UHSettingDialog::ControlVsync()
{
    Config->ToggleVsync();
    Engine->SetResizeReason(UHEngineResizeReason::ToggleVsync);
}

void UHSettingDialog::ControlFullScreen()
{
    Engine->ToggleFullScreen();
}

void UHSettingDialog::ControlCameraSpeed()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.DefaultCameraMoveSpeed = CameraMoveSpeedGUI->Parse<float>();
}

void UHSettingDialog::ControlMouseRotateSpeed()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.MouseRotationSpeed = MouseRotateSpeedGUI->Parse<float>();
}

void UHSettingDialog::ControlForwardKey()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.ForwardKey = ForwardKeyGUI->Parse<char>();
}

void UHSettingDialog::ControlBackKey()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.BackKey = BackKeyGUI->Parse<char>();
}

void UHSettingDialog::ControlLeftKey()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.LeftKey = LeftKeyGUI->Parse<char>();
}

void UHSettingDialog::ControlRightKey()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.RightKey = RightKeyGUI->Parse<char>();
}

void UHSettingDialog::ControlDownKey()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.DownKey = DownKeyGUI->Parse<char>();
}

void UHSettingDialog::ControlUpKey()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.UpKey = UpKeyGUI->Parse<char>();
}

void UHSettingDialog::ControlFPSLimit()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.FPSLimit = FPSLimitGUI->Parse<float>();
}

void UHSettingDialog::ControlBufferMemoryBudget()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.MeshBufferMemoryBudgetMB = MeshMemBudgetGUI->Parse<float>();
}

void UHSettingDialog::ControlImageMemoryBudget()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.ImageMemoryBudgetMB = TexMemBudgetGUI->Parse<float>();
}

void UHSettingDialog::ControlResolution()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    int32_t Width = RenderWidthGUI->Parse<int32_t>();
    int32_t Height = RenderHeightGUI->Parse<int32_t>();

    if (Width > 0 && Height > 0)
    {
        RenderingSettings.RenderWidth = Width;
        RenderingSettings.RenderHeight = Height;
        Engine->SetResizeReason(UHEngineResizeReason::NewResolution);
    }
}

void UHSettingDialog::ControlTAA()
{
    Config->ToggleTAA();
}

void UHSettingDialog::ControlRayTracing()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    RenderingSettings.bEnableRayTracing = !RenderingSettings.bEnableRayTracing;
}

void UHSettingDialog::ControlGPULabeling()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    RenderingSettings.bEnableGPULabeling = !RenderingSettings.bEnableGPULabeling;
}

void UHSettingDialog::ControlLayerValidation()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    RenderingSettings.bEnableLayerValidation = !RenderingSettings.bEnableLayerValidation;
}

void UHSettingDialog::ControlGPUTiming()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    RenderingSettings.bEnableGPUTiming = !RenderingSettings.bEnableGPUTiming;
    GEnableGPUTiming = RenderingSettings.bEnableGPUTiming;
}

void UHSettingDialog::ControlDepthPrePass()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    RenderingSettings.bEnableDepthPrePass = !RenderingSettings.bEnableDepthPrePass;
}

void UHSettingDialog::ControlParallelSubmission()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    RenderingSettings.bParallelSubmission = !RenderingSettings.bParallelSubmission;
}

void UHSettingDialog::ControlParallelThread()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    RenderingSettings.ParallelThreads = ParallelThreadCountGUI->Parse<int32_t>();
}

void UHSettingDialog::ControlShadowQuality()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    RenderingSettings.RTDirectionalShadowQuality = RTShadowQualityGUI->GetSelectedIndex();
    DeferredRenderer->ResizeRayTracingBuffers();
}

void UHSettingDialog::ControlAsyncCompute()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    RenderingSettings.bEnableAsyncCompute = !RenderingSettings.bEnableAsyncCompute;
}

#endif