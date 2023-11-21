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
	void Update();

private:
	UHConfigManager* Config;
	UHEngine* Engine;
	UHDeferredShadingRenderer* DeferredRenderer;
};

#endif