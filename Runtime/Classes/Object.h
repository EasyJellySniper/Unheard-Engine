#pragma once
#include <cstdint>

// UH base object define
class UHObject
{
public:
	UHObject();
	uint32_t GetId() const;

private:
	uint32_t Uid;
};

