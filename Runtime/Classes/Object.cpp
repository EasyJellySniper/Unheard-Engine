#include "Object.h"
#include <assert.h>
#include "Utility.h"
#include "../../UnheardEngine.h"

UHObject::UHObject()
{
	static uint32_t UniqueID = 0;
	Uid = UniqueID++;

	assert(GObjectTable.find(GetId()) == GObjectTable.end());
	GObjectTable[GetId()] = this;
	Name = ENGINE_NAME_NONE;
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

void UHObject::RemoveReferenceObject(UHObject* InObj)
{
	const int32_t RemoveIdx = UHUtilities::FindIndex(GObjectReferences[GetId()], InObj->GetId());
	if (RemoveIdx != UHINDEXNONE)
	{
		UHUtilities::RemoveByIndex(GObjectReferences[GetId()], RemoveIdx);
	}
}

void UHObject::SetName(std::string InName)
{
	Name = InName;
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

std::string UHObject::GetName() const
{
	return Name;
}

bool UHObject::operator==(const UHObject& InObj)
{
	return Uid == InObj.Uid;
}