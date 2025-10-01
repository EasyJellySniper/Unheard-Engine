#pragma once
#include "Dialog.h"

#if WITH_EDITOR
#include <sstream>
#include <iomanip>

class UHProfiler;
class UHGameTimer;
class UHConfigManager;
class UHGraphic;

class UHProfileDialog : public UHDialog
{
public:
	UHProfileDialog();

	virtual void Update(bool& bIsDialogActive) override {}
	void SyncProfileStatistics(UHProfiler* InProfiler, UHGameTimer* InGameTimer, UHConfigManager* InConfig, UHGraphic* InGfx);
	

private:
	std::stringstream CPUStatTex;
	std::stringstream GPUStatTex;
};

#endif