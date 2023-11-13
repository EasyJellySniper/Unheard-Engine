#pragma once
#include "../Classes/Object.h"
#include <vector>
#include <type_traits>
#include "../../UnheardEngine.h"

#if WITH_EDITOR
#include "../../Runtime/Renderer/RenderingTypes.h"
#endif

// base component class of UH, each components are unique
class UHComponent : public UHObject
{
public:
	UHComponent();
	virtual ~UHComponent() {}

	// each component should implement Update() function
	virtual void Update() = 0;

	void SetIsEnabled(bool bInFlag);
	bool IsEnabled() const;

#if WITH_EDITOR
	virtual UHDebugBoundConstant GetDebugBoundConst() const { return UHDebugBoundConstant{}; }
	virtual void OnGenerateDetailView();
	bool IsEditable() const;
#endif

protected:
#if WITH_EDITOR
	bool bIsEditable;
#endif
	bool bIsEnabled;
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
		// check if the object ptr is safe, skip it if it's dangerling
		if (GObjectTable.find(ObjectPair.second->GetId()) == GObjectTable.end())
		{
			continue;
		}

		UHObject* Obj = GObjectTable[ObjectPair.second->GetId()];
		if (T* Comp = dynamic_cast<T*>(Obj))
		{
			OutComponents.push_back(Comp);
		}
	}

	return OutComponents;
}