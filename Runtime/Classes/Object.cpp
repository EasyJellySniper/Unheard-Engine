#include "Object.h"
#include <assert.h>
#include "Utility.h"
#include "../../UnheardEngine.h"

UHObject::UHObject()
{
	static uint32_t UniqueID = 0;
	RuntimeId = UniqueID++;

	assert(GObjectTable.find(GetId()) == GObjectTable.end());
	GObjectTable[GetId()] = this;
	Name = ENGINE_NAME_NONE;

	UuidCreate(&RuntimeGuid);
	Version = 0;
}

void UHObject::OnSave(std::ofstream& OutStream)
{
	OutStream.write(reinterpret_cast<const char*>(&RuntimeGuid), sizeof(RuntimeGuid));
	OutStream.write(reinterpret_cast<const char*>(&Version), sizeof(Version));
	UHUtilities::WriteStringData(OutStream, Name);
}

void UHObject::OnLoad(std::ifstream& InStream)
{
	InStream.read(reinterpret_cast<char*>(&RuntimeGuid), sizeof(RuntimeGuid));
	InStream.read(reinterpret_cast<char*>(&Version), sizeof(Version));
	UHUtilities::ReadStringData(InStream, Name);
}

UHObject::~UHObject()
{
	assert(("Dangling happened, please check the callstack and correct the problematic UObject\n", GObjectTable.find(GetId()) != GObjectTable.end()));
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
	return RuntimeId;
}

UUID UHObject::GetRuntimeGuid() const
{
	return RuntimeGuid;
}

std::string UHObject::GetName() const
{
	return Name;
}

bool UHObject::operator==(const UHObject& InObj)
{
	return RuntimeId == InObj.RuntimeId;
}