#include "Object.h"

UHObject::UHObject()
{
	static uint32_t UniqueID = 0;
	Uid = UniqueID++;
}

uint32_t UHObject::GetId() const
{
	return Uid;
}