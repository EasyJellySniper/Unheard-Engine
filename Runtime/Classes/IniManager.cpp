#include "IniManager.h"

// load an ini file
std::vector<UHIniData> LoadIniFile(std::filesystem::path InPath)
{
	std::vector<UHIniData> Result;
	if (!std::filesystem::exists(InPath))
	{
		return Result;
	}

	// start parsing the ini file
	std::ifstream FileIn(InPath.string().c_str(), std::ios::in);

	// based on stupid read character method
	std::string Buffer;
	char Char;

	UHIniKeyValue IniKeyValue;
	bool bIsKeyFound = false;
	bool bIsValueFound = false;

	while (FileIn.get(Char))
	{
		bool bIsValidBuffer = Buffer.size() > 0;

		if (Char == '[')
		{
			continue;
		}
		else if (Char == ']' && bIsValidBuffer)
		{
			// record a section, special characters need to be removed
			Result.push_back(UHIniData(UHUtilities::RemoveChars(Buffer, "\n\t ")));
			Buffer.clear();
		}
		else if (Char == '=' && bIsValidBuffer)
		{
			// record a key, special characters need to be removed
			IniKeyValue.KeyName = UHUtilities::RemoveChars(Buffer,"\n\t ");
			Buffer.clear();
			bIsKeyFound = true;
		}
		else if (Char == '\n' && bIsValidBuffer)
		{
			// record a value if a newline is met and a key was found previously
			if (bIsKeyFound)
			{
				// value needs to be trimmed
				IniKeyValue.ValueName = UHUtilities::TrimString(Buffer);
				Buffer.clear();
				bIsValueFound = true;
			}
		}
		else if (Char != '\n' && Char != '\t')
		{
			Buffer += Char;
		}

		if (bIsKeyFound && bIsValueFound && Result.size() > 0)
		{
			// push a key value to the result
			Result.back().KeyValue.push_back(IniKeyValue);
			bIsKeyFound = false;
			bIsValueFound = false;
		}
	}

	FileIn.close();
	return Result;
}