#pragma once
#include "Dialog.h"

#if WITH_DEBUG
#include <unordered_map>
#include <memory>
#include "../Controls/CheckBox.h"
#include "../Controls/TextBox.h"
#include "../Controls/Button.h"
#include "../Controls/ComboBox.h"

class UHConfigManager;
class UHEngine;
class UHDeferredShadingRenderer;
class UHRawInput;

class UHSettingDialog : public UHDialog
{
public:
	UHSettingDialog();
	UHSettingDialog(HINSTANCE InInstance, HWND InWindow, UHConfigManager* InConfig, UHEngine* InEngine
		, UHDeferredShadingRenderer* InRenderer, UHRawInput* InRawInput);
	virtual void ShowDialog() override;
	virtual void Update() override;

private:
	// GUI controls
	UniquePtr<UHCheckBox> VSyncGUI;
	UniquePtr<UHCheckBox> FullScreenGUI;
	UniquePtr<UHTextBox> CameraMoveSpeedGUI;
	UniquePtr<UHTextBox> MouseRotateSpeedGUI;
	UniquePtr<UHTextBox> ForwardKeyGUI;
	UniquePtr<UHTextBox> BackKeyGUI;
	UniquePtr<UHTextBox> LeftKeyGUI;
	UniquePtr<UHTextBox> RightKeyGUI;
	UniquePtr<UHTextBox> DownKeyGUI;
	UniquePtr<UHTextBox> UpKeyGUI;
	UniquePtr<UHTextBox> FPSLimitGUI;
	UniquePtr<UHTextBox> MeshMemBudgetGUI;
	UniquePtr<UHTextBox> TexMemBudgetGUI;

	UniquePtr<UHTextBox> RenderWidthGUI;
	UniquePtr<UHTextBox> RenderHeightGUI;
	UniquePtr<UHButton> ApplyResolutionGUI;
	UniquePtr<UHCheckBox>  EnableTaaGUI;
	UniquePtr<UHCheckBox>  EnableRtGUI;
	UniquePtr<UHCheckBox>  EnableGPULabelGUI;
	UniquePtr<UHCheckBox>  EnableLayerValidationGUI;
	UniquePtr<UHCheckBox>  EnableGPUTimingGUI;
	UniquePtr<UHCheckBox>  EnableDepthPrePassGUI;
	UniquePtr<UHCheckBox>  EnableParallelSubmissionGUI;
	UniquePtr<UHTextBox> ParallelThreadCountGUI;
	UniquePtr<UHComboBox>  RTShadowQualityGUI;
	UniquePtr<UHCheckBox>  EnableAsyncComputeGUI;

	// control functions
	void ControlVsync();
	void ControlFullScreen();
	void ControlCameraSpeed();
	void ControlMouseRotateSpeed();
	void ControlForwardKey();
	void ControlBackKey();
	void ControlLeftKey();
	void ControlRightKey();
	void ControlDownKey();
	void ControlUpKey();
	void ControlFPSLimit();
	void ControlBufferMemoryBudget();
	void ControlImageMemoryBudget();
	void ControlResolution();
	void ControlTAA();
	void ControlRayTracing();
	void ControlGPULabeling();
	void ControlLayerValidation();
	void ControlGPUTiming();
	void ControlDepthPrePass();
	void ControlParallelSubmission();
	void ControlParallelThread();
	void ControlShadowQuality();
	void ControlAsyncCompute();

	UHConfigManager* Config;
	UHEngine* Engine;
	UHDeferredShadingRenderer* DeferredRenderer;
	UHRawInput* Input;

	// declare function pointer type for editor control
	std::unordered_map<int32_t, void(UHSettingDialog::*)()> ControlCallbacks;
};

#endif