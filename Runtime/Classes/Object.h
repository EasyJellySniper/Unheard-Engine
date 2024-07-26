#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include <Rpc.h>

// UH base object define
class UHAssetManager;
class UHObject
{
public:
	UHObject();
	virtual ~UHObject();
	virtual void OnSave(std::ofstream& OutStream);
	virtual void OnLoad(std::ifstream& InStream);
	virtual void OnPostLoad(UHAssetManager* InAssetMgr) {}

	void AddReferenceObject(UHObject*);
	void RemoveReferenceObject(UHObject*);
	void SetName(std::string InName);

	std::vector<UHObject*> GetReferenceObjects() const;
	uint32_t GetId() const;
	UUID GetRuntimeGuid() const;
	std::string GetName() const;
	uint32_t GetObjectClassId() const;

	bool operator==(const UHObject& InObj);

protected:
	// runtime GUID, this will always be generated when an object is created
	UUID RuntimeGuid;
	std::string Name;

	// for file versioning
	int32_t Version;
	uint32_t ObjectClassIdInternal;

private:
	// runtime id used for general purpose
	uint32_t RuntimeId;
};

// global table for managing object references
inline std::unordered_map<uint32_t, UHObject*> GObjectTable;
inline std::unordered_map<uint32_t, std::vector<uint32_t>> GObjectReferences;

// safe get object from the table
template <typename T>
inline T* SafeGetObjectFromTable(uint32_t Id)
{
	if (GObjectTable.find(Id) == GObjectTable.end())
	{
		return nullptr;
	}

	return static_cast<T*>(GObjectTable[Id]);
}

template <typename T>
inline T* CastObject(UHObject* InObj)
{
	if (InObj->GetObjectClassId() == T::ClassId)
	{
		return static_cast<T*>(InObj);
	}

	return nullptr;
}

#define STATIC_CLASS_ID(x) static const uint32_t ClassId = x;