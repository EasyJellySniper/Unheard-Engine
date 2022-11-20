#pragma once
#include "../Classes/Object.h"

// base component class of UH, each components are unique
class UHComponent : public UHObject
{
public:
	UHComponent();

	// each component should implement Update() function
	virtual void Update() = 0;
};