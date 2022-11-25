#pragma once
#include "Dialog.h"

#if WITH_DEBUG

class UHProfiler;
class UHGameTimer;
class UHConfigManager;

class UHProfileDialog : public UHDialog
{
public:
	UHProfileDialog();
	UHProfileDialog(HINSTANCE InInstance, HWND InWindow);
	virtual void ShowDialog() override;
	void SyncProfileStatistics(UHProfiler* InProfiler, UHGameTimer* InGameTimer, UHConfigManager* InConfig);
};

#endif