#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vector>
#include <algorithm>
#include <fstream>
#include <memory>
#include <filesystem>
#include <string>

// a header for utilities
namespace UHUtilities
{
	// find an element in a vector
	template<class T>
	inline bool FindByElement(std::vector<T>& InVector, T InElement)
	{
		for (auto& Element : InVector)
		{
			if (Element == InElement)
			{
				return true;
			}
		}

		return false;
	}

	// find an element in a vector, pointer version
	template<class T>
	inline bool FindByElement(std::vector<T*>& InVector, T* InElement)
	{
		for (auto& Element : InVector)
		{
			if (*Element == *InElement)
			{
				return true;
			}
		}

		return false;
	}

	// find element in a vector, unique pointer version
	template<class T>
	inline bool FindByElement(std::vector<std::unique_ptr<T>>& InVector, T InElement)
	{
		for (auto& Element : InVector)
		{
			// in case the input T is a pointer, needs to compare value
			if (*Element == InElement)
			{
				return true;
			}
		}

		return false;
	}

	// find index by element in a vector
	template<class T>
	inline int32_t FindIndex(std::vector<T>& InVector, T InElement)
	{
		for (size_t Idx = 0; Idx < InVector.size(); Idx++)
		{
			// in case the input T is a pointer, needs to compare value
			if (InVector[Idx] == InElement)
			{
				return static_cast<int32_t>(Idx);
			}
		}

		return -1;
	}

	// find index by element in a vector, pointer version
	template<class T>
	inline int32_t FindIndex(std::vector<T*>& InVector, T* InElement)
	{
		for (size_t Idx = 0; Idx < InVector.size(); Idx++)
		{
			// in case the input T is a pointer, needs to compare value
			if (*InVector[Idx] == *InElement)
			{
				return static_cast<int32_t>(Idx);
			}
		}

		return -1;
	}

	// find index by element in a vector, unique pointer version
	template<class T>
	inline int32_t FindIndex(std::vector<std::unique_ptr<T>>& InVector, T InElement)
	{
		for (size_t Idx = 0; Idx < InVector.size(); Idx++)
		{
			// in case the input T is a pointer, needs to compare value
			if (*InVector[Idx] == InElement)
			{
				return static_cast<int32_t>(Idx);
			}
		}

		return -1;
	}

	// remove by index
	template<class T>
	inline void RemoveByIndex(std::vector<T>& InVector, int32_t InIndex)
	{
		InVector.erase(InVector.begin() + InIndex);
	}

	// generic write string data
	inline void WriteStringData(std::ofstream& FileOut, std::string InString)
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
	inline void ReadStringData(std::ifstream& FileIn, std::string& OutString)
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

	// generic write vector data
	template<class T>
	inline void WriteVectorData(std::ofstream& FileOut, std::vector<T>& InVector)
	{
		// don't write if file it's not opened
		if (FileOut.fail())
		{
			return;
		}

		size_t ElementCount = InVector.size();
		FileOut.write(reinterpret_cast<const char*>(&ElementCount), sizeof(ElementCount));
		FileOut.write(reinterpret_cast<const char*>(&InVector.data()[0]), ElementCount * sizeof(T));
	}

	// generic read vector data
	template<class T>
	inline void ReadVectorData(std::ifstream& FileIn, std::vector<T>& OutVector)
	{
		// don't read if file it's not opended
		if (FileIn.fail())
		{
			return;
		}

		// file must've written "Element counts" or this will fail
		
		// read element count first
		size_t ElementCount;
		FileIn.read(reinterpret_cast<char*>(&ElementCount), sizeof(ElementCount));

		OutVector.resize(ElementCount);
		FileIn.read(reinterpret_cast<char*>(&OutVector.data()[0]), ElementCount * sizeof(T));
	}

	// find properties of a memory, mirror from Vulkan spec
	inline int32_t FindMemoryTypes(const VkPhysicalDeviceMemoryProperties* MemoryProperties,
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

	inline std::string ToStringA(std::wstring InStringW)
	{
		return std::filesystem::path(InStringW).string();
	}

	inline std::wstring ToStringW(std::string InStringA)
	{
		return std::filesystem::path(InStringA).wstring();
	}

	inline std::string RemoveChars(std::string InString, std::string InChars)
	{
		for (size_t Idx = 0; Idx < InChars.length(); Idx++)
		{
			std::string::iterator EndPos = std::remove(InString.begin(), InString.end(), InChars[Idx]);
			InString.erase(EndPos, InString.end());
		}

		return InString;
	}

	inline std::string RemoveSubString(std::string InString, std::string InSubString)
	{
		return InString.erase(InString.find(InSubString), InSubString.length());
	}

	inline void WriteINISection(std::ofstream& FileOut, std::string InSection)
	{
		FileOut << "[" << InSection << "]\n";
	}

	template<typename T>
	inline void WriteINIData(std::ofstream& FileOut, std::string Key, T Value)
	{
		FileOut << Key << "=" << std::to_string(Value) << std::endl;
	}

	inline bool SeekINISection(std::ifstream& FileIn, std::string Section)
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

	template<typename T>
	inline T ReadINIData(std::ifstream& FileIn, std::string Key)
	{
		// this function simply reads ini data, doesn't consider comment yet
		// works on number value only as well
		// assume SeekSection() is already called

		std::string KeyFound;
		std::string ValueFound;

		std::string Line;
		while (std::getline(FileIn, Line))
		{
			Line = RemoveChars(Line, " \t");

			size_t KeyPos = Line.find('=');
			if (KeyPos != std::string::npos)
			{
				KeyFound = Line.substr(0, KeyPos);
				ValueFound = Line.substr(KeyPos + 1, Line.length() - KeyPos - 1);

				if (KeyFound == Key)
				{
					// return value with string to double anyway
					// then cast it to target type
					return static_cast<T>(std::stod(ValueFound));
				}
			}
		}

		// no value found, also the worst case of this reader
		return -1;
	}

	// djb2 string to hash, reference: http://www.cse.yorku.ca/~oz/hash.html
	inline size_t StringToHash(std::string InString)
	{
		size_t Hash = 5381;

		for (const char& C : InString)
		{
			Hash = ((Hash << 5) + Hash) + C; /* Hash * 33 + c */
		}
		return Hash;
	}

	// inline function for convert shader defines to hash
	inline size_t ShaderDefinesToHash(std::vector<std::string> Defines)
	{
		std::string MacroString;
		for (const std::string& Str : Defines)
		{
			MacroString += Str;
		}

		size_t MacroHash = (MacroString.size() > 0) ? UHUtilities::StringToHash(MacroString) : 0;
		return MacroHash;
	}

	inline std::string ToLowerString(std::string InString)
	{
		std::for_each(InString.begin(), InString.end(), [](char& C)
			{
				C = std::tolower(static_cast<unsigned char>(C));
			});

		return InString;
	}

	inline bool StringFind(std::string InString, std::string InSearch)
	{
		return InString.find(InSearch) != std::string::npos;
	}
}