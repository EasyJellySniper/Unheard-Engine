#include "Object.h"
#include <assert.h>
#include "Utility.h"

UHObject::UHObject()
{
	static uint32_t UniqueID = 0;
	Uid = UniqueID++;

	assert(GObjectTable.find(GetId()) == GObjectTable.end());
	GObjectTable[GetId()] = this;
}

UHObject::~UHObject()
{
	GObjectTable.erase(GetId());
}

void UHObject::AddReferenceObject(UHObject* InObj)
{
	if (!UHUtilities::FindByElement(GObjectReferences[GetId()], InObj->GetId()))
	{
		GObjectReferences[GetId()].push_back(InObj->GetId());
	}
}

std::vector<UHObject*> UHObject::GetReferenceObjects() const
{
	std::vector<UHObject*> References;

	// if the object has any references
	uint32_t ObjId = GetId();
	if (GObjectReferences.find(ObjId) != GObjectReferences.end())
	{
		for (int32_t Idx = static_cast<int32_t>(GObjectReferences[ObjId].size()) - 1; Idx >= 0; Idx--)
		{
			uint32_t TargetId = GObjectReferences[ObjId][Idx];

			// collect references if target is still existed
			if (GObjectTable.find(TargetId) != GObjectTable.end())
			{
				References.push_back(GObjectTable[TargetId]);
			}
			else
			{
				UHUtilities::RemoveByIndex(GObjectReferences[ObjId], Idx);
			}
		}
	}

	return References;
}

uint32_t UHObject::GetId() const
{
	return Uid;
}