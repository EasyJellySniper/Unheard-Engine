#include "Slider.h"

#if WITH_EDITOR
#include <CommCtrl.h>

UHSlider::UHSlider(HWND InControl, UHGUIProperty InProperties)
	: UHGUIControlBase(InControl, InProperties)
{

}

UHSlider& UHSlider::Range(uint32_t MinVal, uint32_t MaxVal)
{
	SendMessage(ControlObj, TBM_SETRANGE,
		(WPARAM)TRUE,
		(LPARAM)MAKELONG(MinVal, MaxVal));

	return *this;
}

UHSlider& UHSlider::SetPos(uint32_t InPos)
{
	SendMessage(ControlObj, TBM_SETPOS,
		(WPARAM)TRUE,
		(LPARAM)InPos);

	return *this;
}

uint32_t UHSlider::GetPos() const
{
	return static_cast<uint32_t>(SendMessage(ControlObj, TBM_GETPOS, 0, 0));
}

#endif