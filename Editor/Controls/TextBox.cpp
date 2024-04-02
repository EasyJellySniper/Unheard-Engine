#include "TextBox.h"

#if WITH_EDITOR
#include "../../../../Runtime/Classes/Utility.h"
#include <windowsx.h>

UHTextBox::UHTextBox()
	: UHGUIControlBase(nullptr, UHGUIProperty())
{

}

UHTextBox::UHTextBox(HWND InControl, UHGUIProperty InProperties)
	: UHGUIControlBase(InControl, InProperties)
{

}

UHTextBox::UHTextBox(std::string InGUIName, UHGUIProperty InProperties)
{
	DWORD Style = WS_BORDER | WS_CHILD | ES_AUTOHSCROLL;
	if (InProperties.bClipped)
	{
		Style |= WS_CLIPCHILDREN;
	}

	ControlObj = CreateWindowA("EDIT", "0", Style, InProperties.InitPosX, InProperties.InitPosY
		, InProperties.InitWidth, InProperties.InitHeight, InProperties.ParentWnd, NULL, InProperties.Instance, NULL);

	InternalInit(ControlObj, InProperties);
	bIsManuallyCreated = true;
}

// template function to set text
template <typename T>
UHTextBox& UHTextBox::Content(T InValue)
{
	bIsSetFromCode = true;

	if constexpr(std::is_same<T, char>::value)
	{
		std::string InStrA;
		InStrA.push_back(InValue);
		UHGUIControlBase::SetText(UHUtilities::ToStringW(InStrA));
	}
	else if constexpr (std::is_same<T, float>::value)
	{
		UHGUIControlBase::SetText(UHUtilities::FloatToWString(InValue));
	}
	else
	{
		UHGUIControlBase::SetText(std::to_wstring(InValue));
	}

	return *this;
}

// template function to parse the textbox content
template <typename T>
T UHTextBox::Parse()
{
	T Result{};

	try
	{
		if (std::is_same<T, int32_t>::value)
		{
			Result = (T)std::stoi(GetText());
		}
		else if (std::is_same<T, float>::value)
		{
			Result = (T)std::stof(GetText());
		}
		else if (std::is_same<T, double>::value)
		{
			Result = (T)std::stod(GetText());
		}
		else if (std::is_same<T, bool>::value)
		{
			Result = (T)(std::stoi(GetText()));
		}
		else if (std::is_same<T, char>::value)
		{
			std::string AString = UHUtilities::ToStringA(GetText());
			if (AString.length() == 1)
			{
				Result = AString[0];
			}
			else
			{
				throw std::runtime_error("Format error");
			}
		}
		else
		{
			throw std::runtime_error("Format error");
		}
	}
	catch (...)
	{
		UHE_LOG((L"Parsing " + GetText() + L" failed!"));
	}

	return Result;
}

template UHTextBox& UHTextBox::Content(int32_t);
template UHTextBox& UHTextBox::Content(float);
template UHTextBox& UHTextBox::Content(double);
template UHTextBox& UHTextBox::Content(bool);
template UHTextBox& UHTextBox::Content(char);

template int32_t UHTextBox::Parse<int32_t>();
template float UHTextBox::Parse<float>();
template double UHTextBox::Parse<double>();
template bool UHTextBox::Parse<bool>();
template char UHTextBox::Parse<char>();

#endif