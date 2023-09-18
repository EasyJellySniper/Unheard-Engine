#pragma once
#include "../Classes/Object.h"
#include <vector>
#include <type_traits>
#include "../../UnheardEngine.h"

#if WITH_EDITOR
#include "../../Runtime/Renderer/RenderingTypes.h"
#include "../../Editor/Classes/DetailView.h"
#include "../../Editor/Classes/Reflection.h"

#define UH_SYNC_DETAIL_VALUE(InPropName, InVal) \
if (DetailView) \
DetailView->SetValue(InPropName, InVal);

#else
#define UH_SYNC_DETAIL_VALUE(InPropName, InVal)
#endif

// base component class of UH, each components are unique
class UHComponent : public UHObject
{
public:
	UHComponent();
	virtual ~UHComponent() {}

	// each component should implement Update() function
	virtual void Update() = 0;

#if WITH_EDITOR
	virtual void OnPropertyChange(std::string PropertyName) {}
	virtual UHDebugBoundConstant GetDebugBoundConst() const { return UHDebugBoundConstant{}; }
	virtual void OnGenerateDetailView(HWND ParentWindow) { }
	bool IsEditable() const;
#endif

protected:
#if WITH_EDITOR
	bool bIsEditable;
	int32_t DetailStartHeight;
#endif
};

template <typename T>
inline std::vector<T*> GetComponents()
{
	std::vector<T*> OutComponents;

	if (!std::is_same<UHComponent, T>::value || !std::is_base_of<UHComponent, T>::value)
	{
		return OutComponents;
	}

	for (auto& ObjectPair : GObjectTable)
	{
		if (T* Comp = dynamic_cast<T*>(ObjectPair.second))
		{
			OutComponents.push_back(Comp);
		}
	}

	return OutComponents;
}