#pragma once
#include "../../UnheardEngine.h"

#if WITH_DEBUG

// UH shared Dialog class, needs to implement ShowDialog in child class
class UHDialog
{
public:
	UHDialog(HINSTANCE InInstance, HWND InWindow);
	virtual void ShowDialog() = 0;

protected:
	HINSTANCE Instance;
	HWND Window;
};

#endif