#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>

// UH base object define
class UHObject
{
public:
	UHObject();
	virtual ~UHObject();

	void AddReferenceObject(UHObject*);
	void RemoveReferenceObject(UHObject*);
	void SetName(std::string InName);

	std::vector<UHObject*> GetReferenceObjects() const;
	uint32_t GetId() const;
	std::string GetName() const;

	bool operator==(const UHObject& InMat);

private:
	uint32_t Uid;
	std::string Name;
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