#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>

// UH base object define
class UHObject
{
public:
	UHObject();
	~UHObject();

	void AddReferenceObject(UHObject*);
	std::vector<UHObject*> GetReferenceObjects() const;

	uint32_t GetId() const;

	bool operator==(const UHObject& InMat);

private:
	uint32_t Uid;
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