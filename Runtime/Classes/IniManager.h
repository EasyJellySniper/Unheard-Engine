#pragma once
#include "../../UnheardEngine.h"

// manager for ini files
struct UHIniKeyValue
{
	UHIniKeyValue()
	{

	}

	UHIniKeyValue(std::string InKey, std::string InValue)
		: KeyName(InKey)
		, ValueName(InValue)
	{

	}

	std::string KeyName;
	std::string ValueName;
};

struct UHIniData
{
	UHIniData(std::string InSection)
		: SectionName(InSection)
	{

	}

	std::string SectionName;
	std::vector<UHIniKeyValue> KeyValue;
};

extern std::vector<UHIniData> LoadIniFile(std::filesystem::path InPath);