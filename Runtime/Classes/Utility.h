#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vector>
#include <algorithm>
#include <fstream>
#include <memory>
#include <filesystem>
#include <string>
#include <sstream>

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
	inline bool FindByElement(const std::vector<T*>& InVector, T* InElement)
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
	inline bool FindByElement(const std::vector<std::unique_ptr<T>>& InVector, T InElement)
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
	inline int32_t FindIndex(const std::vector<T>& InVector, T InElement)
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
	inline int32_t FindIndex(const std::vector<T*>& InVector, T* InElement)
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
	inline int32_t FindIndex(const std::vector<std::unique_ptr<T>>& InVector, T InElement)
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
	inline void RemoveByIndex(std::vector<T>& InVector, int32_t InIndex, int32_t InLast = -1)
	{
		if (InLast == -1)
		{
			InVector.erase(InVector.begin() + InIndex);
		}
		else
		{
			InVector.erase(InVector.begin() + InIndex, InVector.begin() + InLast);
		}
	}

	// generic write string data
	void WriteStringData(std::ofstream& FileOut, std::string InString);

	// generic read string data
	void ReadStringData(std::ifstream& FileIn, std::string& OutString);

	// generic write vector data
	template<class T>
	inline void WriteVectorData(std::ofstream& FileOut, const std::vector<T>& InVector)
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

	void WriteStringVectorData(std::ofstream& FileOut, std::vector<std::string>& InVector);
	void ReadStringVectorData(std::ifstream& FileIn, std::vector<std::string>& OutVector);

	// find properties of a memory, mirror from Vulkan spec
	int32_t FindMemoryTypes(const VkPhysicalDeviceMemoryProperties* MemoryProperties,
		uint32_t MemoryTypeBitsRequirement,
		VkMemoryPropertyFlags RequiredProperties);

	std::string ToStringA(std::wstring InStringW);

	std::wstring ToStringW(std::string InStringA);

	std::string RemoveChars(std::string InString, std::string InChars);

	std::string RemoveSubString(std::string InString, std::string InSubString);

	void WriteINISection(std::ofstream& FileOut, std::string InSection);

	template<typename T>
	inline void WriteINIData(std::ofstream& FileOut, std::string Key, T Value)
	{
		FileOut << Key << "=" << std::to_string(Value) << std::endl;
	}

	bool SeekINISection(std::ifstream& FileIn, std::string Section);

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
	size_t StringToHash(std::string InString);

	// inline function for convert shader defines to hash
	size_t ShaderDefinesToHash(std::vector<std::string> Defines);

	std::string ToLowerString(std::string InString);

	bool StringFind(std::string InString, std::string InSearch);

	size_t StringFindIndex(std::string InString, std::string InSearch);

	std::string StringReplace(std::string InString, std::string InKeyWord, std::string InValue);

	std::wstring FloatToWString(float InValue, int32_t InPrecision = 2);

	std::string FloatToString(float InValue, int32_t InPrecision = 2);
}