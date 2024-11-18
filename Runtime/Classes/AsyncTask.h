#pragma once
#include "../../UnheardEngine.h"

// UHE async task class, typically referenced by UHThread
class UHAsyncTask
{
public:
	virtual void DoTask(const int32_t ThreadIndex) {}
};