#pragma once
#include "Dialog.h"

// simple dialog for showing the status, such as "Loading, Processing..etc"
// for now it's simply used in scope
#if WITH_EDITOR
class UHStatusDialogScope : public UHDialog
{
public:
	UHStatusDialogScope(std::string InMsg);
	~UHStatusDialogScope();

	virtual void ShowDialog() override {}
	virtual void Update(bool& bIsDialogActive) override {}
};
#else
class UHStatusDialogScope
{
public:
	UHStatusDialogScope(std::string InMsg)
	{

	}
};
#endif