#include "Utility.h"
#include <unordered_map>

namespace UHUtilities
{
	std::unordered_map<size_t, size_t> IniSectionCache;

	// generic write string data
	void WriteStringData(std::ofstream& FileOut, const std::string& InString)
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
		size_t StringSize = 0;
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
		size_t VectorSize = 0;
		FileIn.read(reinterpret_cast<char*>(&VectorSize), sizeof(VectorSize));

		OutVector.resize(VectorSize);
		for (size_t Idx = 0; Idx < VectorSize; Idx++)
		{
			ReadStringData(FileIn, OutVector[Idx]);
		}
	}

	std::string ToStringA(const std::wstring& InStringW)
	{
		return std::filesystem::path(InStringW).string();
	}

	std::wstring ToStringW(const std::string& InStringA)
	{
		const int32_t WStringSize = MultiByteToWideChar(CP_UTF8, 0, &InStringA[0], (int32_t)InStringA.size(), NULL, 0);
		std::wstring WString(WStringSize, 0);
		MultiByteToWideChar(CP_UTF8, 0, &InStringA[0], (int32_t)InStringA.size(), &WString[0], WStringSize);

		return WString;
	}

	std::string RemoveChars(std::string InString, const std::string& InChars)
	{
		for (size_t Idx = 0; Idx < InChars.length(); Idx++)
		{
			std::string::iterator EndPos = std::remove(InString.begin(), InString.end(), InChars[Idx]);
			InString.erase(EndPos, InString.end());
		}

		return InString;
	}

	std::string RemoveSubString(std::string InString, const std::string& InSubString)
	{
		size_t Pos = InString.find(InSubString);
		if (Pos == std::string::npos)
		{
			return InString;
		}

		return InString.erase(Pos, InSubString.length());
	}

	std::string TrimString(const std::string& InString)
	{
		// early out if no contents
		if (InString.empty())
		{
			return InString;
		}

		const std::string& Whitespace = " \t";
		const auto StringBegin = InString.find_first_not_of(Whitespace);
		const auto StringEnd = InString.find_last_not_of(Whitespace);
		const auto StringRange = StringEnd - StringBegin + 1;

		return InString.substr(StringBegin, StringRange);
	}

	void WriteINISection(std::ofstream& FileOut, std::string InSection)
	{
		FileOut << "[" << InSection << "]\n";
	}

	size_t SeekINISection(std::ifstream& FileIn, std::string Section)
	{
		// this function will move the file pos to section
		FileIn.seekg(0);

		// early out if section has been cached
		const size_t SectionHash = StringToHash(Section);
		const auto SectionHashIter = IniSectionCache.find(SectionHash);
		if (SectionHashIter != IniSectionCache.end())
		{
			return SectionHashIter->second;
		}

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
				// cache the section pos
				const size_t CurrentPos = FileIn.tellg();
				IniSectionCache[StringToHash(Section)] = CurrentPos;
				return CurrentPos;
			}
		}

		return std::string::npos;
	}

	// djb2 string to hash, reference: http://www.cse.yorku.ca/~oz/hash.html
	size_t StringToHash(const std::string& InString)
	{
		size_t Hash = 5381;

		for (const char& C : InString)
		{
			Hash = ((Hash << 5) + Hash) + C; /* Hash * 33 + c */
		}
		return Hash;
	}

	// inline function for convert shader defines to hash
	size_t ShaderDefinesToHash(const std::vector<std::string>& Defines)
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

	bool StringFind(const std::string& InString, const std::string& InSearch)
	{
		return InString.find(InSearch) != std::string::npos;
	}

	size_t StringFindIndex(const std::string& InString, const std::string& InSearch)
	{
		return InString.find(InSearch);
	}

	std::string StringReplace(std::string InString, const std::string& InKeyWord, const std::string& InValue)
	{
		size_t MarkerPos = StringFindIndex(InString, InKeyWord);
		while (MarkerPos != std::string::npos)
		{
			InString = InString.replace(MarkerPos, InKeyWord.size(), InValue);
			MarkerPos = StringFindIndex(InString, InKeyWord);
		}

		return InString;
	}

	std::wstring FloatToWString(const float InValue, const int32_t InPrecision)
	{
		std::wstringstream Stream;
		Stream << std::fixed << std::setprecision(InPrecision) << InValue;

		return Stream.str();
	}

	std::string FloatToString(const float InValue, const int32_t InPrecision)
	{
		std::stringstream Stream;
		Stream << std::fixed << std::setprecision(InPrecision) << InValue;

		return Stream.str();
	}
}