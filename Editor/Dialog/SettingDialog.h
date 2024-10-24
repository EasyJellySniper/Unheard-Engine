#pragma once
#include "Dialog.h"

#if WITH_EDITOR
#include <unordered_map>
#include <memory>

class UHConfigManager;
class UHEngine;
class UHDeferredShadingRenderer;

class UHSettingDialog : public UHDialog
{
public:
	UHSettingDialog(UHConfigManager* InConfig, UHEngine* InEngine, UHDeferredShadingRenderer* InRenderer);
	virtual void ShowDialog() override;
	virtual void Update(bool& bIsDialogActive) override;

private:
	UHConfigManager* Config;
	UHEngine* Engine;
	UHDeferredShadingRenderer* DeferredRenderer;

	static int32_t TempWidth;
	static int32_t TempHeight;
};

#endif