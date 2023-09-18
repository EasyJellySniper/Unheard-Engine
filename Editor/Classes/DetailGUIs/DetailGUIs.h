#pragma once

#if WITH_EDITOR
#include "../../../Editor/Controls/GroupBox.h"
#include "../../../Editor/Controls/Label.h"
#include "../../../Editor/Controls/TextBox.h"
#include "../../../Editor/Controls/CheckBox.h"
#include "../../../Editor/Controls/ComboBox.h"
#include "../../../Editor/Classes/EditorUtils.h"
#include "../../../Runtime/Classes/Types.h"
#include <functional>

// detail GUIs implementation, aim for automation
class UHDetailGUIBase
{
public:
	UHDetailGUIBase(std::string InName);
	virtual ~UHDetailGUIBase() {}
	virtual void BindCallback(std::function<void(std::string)> InCallback) = 0;

	std::string GetName() const;

private:
	std::string Name;
};

extern UniquePtr<UHDetailGUIBase> GetDetailGUI(const std::string InType, const std::string InName, const UHGUIProperty InProps);

#endif