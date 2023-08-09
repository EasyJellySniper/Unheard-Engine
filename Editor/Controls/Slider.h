#pragma once
#include "GUIControl.h"

#if WITH_DEBUG

class UHSlider : public UHGUIControlBase
{
public:
	UHSlider(HWND InControl, UHGUIProperty InProperties = UHGUIProperty());

	UHSlider& Range(uint32_t MinVal, uint32_t MaxVal);
	UHSlider& SetPos(uint32_t InPos);
	uint32_t GetPos() const;
};

#endif