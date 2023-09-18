#pragma once
#include "../../UnheardEngine.h"

#if WITH_EDITOR
#include <typeindex>
#include "../../Editor/Classes/Reflection.h"
#include "../../Editor/Controls/GroupBox.h"
#include "DetailGUIs/Float3DetailGUI.h"
#include "DetailGUIs/FloatDetailGUI.h"
#include "DetailGUIs/BoolDetailGUI.h"
#include "DetailGUIs/DropListDetailGUI.h"

class UHDetailView
{
public:
	UHDetailView(std::string InName)
		: DetailViewName(InName)
	{
	}

	virtual ~UHDetailView() 
	{
		DetailGUIs.clear();
	}

	template <typename T>
	void OnGenerateDetailView(T* InComp, HWND ParentWnd, int32_t& StartHeight);

	template <typename T>
	void SetValue(std::string PropertyName, T InVal);

	template <typename T>
	T GetValue(std::string PropertyName) const;

	UHDetailGUIBase* GetGUI(std::string InName) const;

private:
	std::string DetailViewName;

	UniquePtr<UHGroupBox> DetailBlock;
	std::vector<UniquePtr<UHDetailGUIBase>> DetailGUIs;
};

#endif