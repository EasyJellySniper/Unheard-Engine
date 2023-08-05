#include "Utility.h"

namespace UHUtilities
{
	// generic write string data
	void WriteStringData(std::ofstream& FileOut, std::string InString)
	{
		// don't write if file it's not opened
		if (FileOut.fail())
		{
			return;
		}

		size_t StringSize = InString.size();
		FileOut.write(reinterpret_cast<const char*>(&StringSize), sizeof(StringSize));
		FileOut.write(InString.c_str(), StringSize);
	}

	// generic read string data
	void ReadStringData(std::ifstream& FileIn, std::string& OutString)
	{
		// don't read if file it's not opened
		if (FileIn.fail())
		{
			return;
		}

		// file must've written "string size" or this might fail
		size_t StringSize;
		FileIn.read(reinterpret_cast<char*>(&StringSize), sizeof(StringSize));

		// create a char array buffer and read to string
		char* TempBuffer = new char[StringSize + 1];
		FileIn.read(TempBuffer, StringSize);
		TempBuffer[StringSize] = '\0';

		OutString = TempBuffer;
		delete[]TempBuffer;
	}

	void WriteStringVectorData(std::ofstream& FileOut, std::vector<std::string>& InVector)
	{
		size_t VectorSize = InVector.size();
		FileOut.write(reinterpret_cast<const char*>(&VectorSize), sizeof(VectorSize));

		for (size_t Idx = 0; Idx < VectorSize; Idx++)
		{
			WriteStringData(FileOut, InVector[Idx]);
		}
	}

	void ReadStringVectorData(std::ifstream& FileIn, std::vector<std::string>& OutVector)
	{
		size_t VectorSize;
		FileIn.read(reinterpret_cast<char*>(&VectorSize), sizeof(VectorSize));

		OutVector.resize(VectorSize);
		for (size_t Idx = 0; Idx < VectorSize; Idx++)
		{
			ReadStringData(FileIn, OutVector[Idx]);
		}
	}

	// find properties of a memory, mirror from Vulkan spec
	int32_t FindMemoryTypes(const VkPhysicalDeviceMemoryProperties* MemoryProperties,
		uint32_t MemoryTypeBitsRequirement,
		VkMemoryPropertyFlags RequiredProperties)
	{
		const uint32_t MemoryCount = MemoryProperties->memoryTypeCount;
		for (uint32_t MemoryIndex = 0; MemoryIndex < MemoryCount; ++MemoryIndex)
		{
			const uint32_t MemoryTypeBits = (1 << MemoryIndex);
			const bool bIsRequiredMemoryType = MemoryTypeBitsRequirement & MemoryTypeBits;

			const VkMemoryPropertyFlags Properties =
				MemoryProperties->memoryTypes[MemoryIndex].propertyFlags;
			const bool bHasRequiredProperties =
				(Properties & RequiredProperties) == RequiredProperties;

			if (bIsRequiredMemoryType && bHasRequiredProperties)
				return static_cast<int32_t>(MemoryIndex);
		}

		// failed to find memory type
		return -1;
	}

	std::string ToStringA(std::wstring InStringW)
	{
		return std::filesystem::path(InStringW).string();
	}

	std::wstring ToStringW(std::string InStringA)
	{
		return std::filesystem::path(InStringA).wstring();
	}

	std::string RemoveChars(std::string InString, std::string InChars)
	{
		for (size_t Idx = 0; Idx < InChars.length(); Idx++)
		{
			std::string::iterator EndPos = std::remove(InString.begin(), InString.end(), InChars[Idx]);
			InString.erase(EndPos, InString.end());
		}

		return InString;
	}

	std::string RemoveSubString(std::string InString, std::string InSubString)
	{
		size_t Pos = InString.find(InSubString);
		if (Pos == std::string::npos)
		{
			return InString;
		}

		return InString.erase(Pos, InSubString.length());
	}

	void WriteINISection(std::ofstream& FileOut, std::string InSection)
	{
		FileOut << "[" << InSection << "]\n";
	}

	bool SeekINISection(std::ifstream& FileIn, std::string Section)
	{
		// this function will move the file pos to section
		FileIn.seekg(0);

		std::string Line;
		std::string SectionFound;
		while (std::getline(FileIn, Line))
		{
			Line = RemoveChars(Line, " \t");

			if (Line.find('[') != std::string::npos && Line.find(']') != std::string::npos)
			{
				SectionFound = Line.substr(1, Line.length() - 2);
			}

			if (SectionFound == Section)
			{
				return true;
			}
		}

		return false;
	}

	// djb2 string to hash, reference: http://www.cse.yorku.ca/~oz/hash.html
	size_t StringToHash(std::string InString)
	{
		size_t Hash = 5381;

		for (const char& C : InString)
		{
			Hash = ((Hash << 5) + Hash) + C; /* Hash * 33 + c */
		}
		return Hash;
	}

	// inline function for convert shader defines to hash
	size_t ShaderDefinesToHash(std::vector<std::string> Defines)
	{
		std::string MacroString;
		for (const std::string& Str : Defines)
		{
			MacroString += Str;
		}

		size_t MacroHash = (MacroString.size() > 0) ? UHUtilities::StringToHash(MacroString) : 0;
		return MacroHash;
	}

	std::string ToLowerString(std::string InString)
	{
		std::for_each(InString.begin(), InString.end(), [](char& C)
			{
				C = std::tolower(static_cast<unsigned char>(C));
			});

		return InString;
	}

	bool StringFind(std::string InString, std::string InSearch)
	{
		return InString.find(InSearch) != std::string::npos;
	}

	size_t StringFindIndex(std::string InString, std::string InSearch)
	{
		return InString.find(InSearch);
	}

	std::string StringReplace(std::string InString, std::string InKeyWord, std::string InValue)
	{
		size_t MarkerPos = StringFindIndex(InString, InKeyWord);
		while (MarkerPos != std::string::npos)
		{
			InString = InString.replace(MarkerPos, InKeyWord.size(), InValue);
			MarkerPos = StringFindIndex(InString, InKeyWord);
		}

		return InString;
	}

	std::wstring FloatToWString(float InValue, int32_t InPrecision)
	{
		std::wstringstream Stream;
		Stream << std::fixed << std::setprecision(InPrecision) << InValue;

		return Stream.str();
	}

	std::string FloatToString(float InValue, int32_t InPrecision)
	{
		std::stringstream Stream;
		Stream << std::fixed << std::setprecision(InPrecision) << InValue;

		return Stream.str();
	}
}