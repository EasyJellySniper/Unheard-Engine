#include "Utility.h"
#include <unordered_map>

#if _WIN32
#include <Windows.h>
#elif __linux__
#include <iconv.h>
#endif

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

		uint64_t StringSize = InString.size();
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
		uint64_t StringSize = 0;
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
		uint64_t VectorSize = InVector.size();
		FileOut.write(reinterpret_cast<const char*>(&VectorSize), sizeof(VectorSize));

		for (size_t Idx = 0; Idx < VectorSize; Idx++)
		{
			WriteStringData(FileOut, InVector[Idx]);
		}
	}

	void ReadStringVectorData(std::ifstream& FileIn, std::vector<std::string>& OutVector)
	{
		uint64_t VectorSize = 0;
		FileIn.read(reinterpret_cast<char*>(&VectorSize), sizeof(VectorSize));

		OutVector.resize(VectorSize);
		for (size_t Idx = 0; Idx < VectorSize; Idx++)
		{
			ReadStringData(FileIn, OutVector[Idx]);
		}
	}

	std::string ToStringA(const std::wstring& InStringW)
	{
		// platform-based string conversion

#if _WIN32

		const int32_t AStringsize = WideCharToMultiByte(CP_UTF8, 0, &InStringW[0], (int32_t)InStringW.size(), nullptr, 0, nullptr, nullptr);
		std::string AString(AStringsize, 0);
		WideCharToMultiByte(CP_UTF8, 0, &InStringW[0], (int32_t)InStringW.size(), &AString[0], AStringsize, nullptr, nullptr);

#elif __linux__

		iconv_t Conv = iconv_open("UTF-8", "WCHAR_T");
		char* InBuff = reinterpret_cast<char*>(const_cast<wchar_t*>(InStringW.data()));
		size_t InBytes = InStringW.size() * sizeof(wchar_t);

		std::vector<char> OutBuff(InStringW.size() * 4); // worst-case UTF-8 expansion
		char* OutPtr = OutBuff.data();
		size_t OutBytes = OutBuff.size();

		size_t Res = iconv(Conv, &InBuff, &InBytes, &OutPtr, &OutBytes);
		iconv_close(Conv);

		if (Res == (size_t)-1)
		{
			// could be E2BIG or EILSEQ or EINVAL
			// deal with those cases afterward if necessary
			return "";
		}

		const size_t BytesWritten = OutBuff.size() - OutBytes;
		std::string AString(OutBuff.data(), BytesWritten);
#else
		// just a fallback for unknown platform
		// this is not a good solution, better to identify the platform and implement corresponding conversion
		std::string AString(InStringW.begin(), InStringW.end());
#endif

		return AString;
	}

	std::wstring ToStringW(const std::string& InStringA)
	{
		// platform-based string conversion

#if _WIN32

		const int32_t WStringSize = MultiByteToWideChar(CP_UTF8, 0, &InStringA[0], (int32_t)InStringA.size(), nullptr, 0);
		std::wstring WString(WStringSize, 0);
		MultiByteToWideChar(CP_UTF8, 0, &InStringA[0], (int32_t)InStringA.size(), &WString[0], WStringSize);

#elif __linux__

		iconv_t Conv = iconv_open("WCHAR_T", "UTF-8");
		char* InBuff = const_cast<char*>(InStringA.data());
		size_t InBytes = InStringA.size();

		std::vector<wchar_t> OutBuff(InStringA.size() * 2);
		char* OutPtr = reinterpret_cast<char*>(OutBuff.data());
		size_t OutBytes = OutBuff.size() * sizeof(wchar_t);

		size_t Res = iconv(Conv, &InBuff, &InBytes, &OutPtr, &OutBytes);
		iconv_close(Conv);

		if (Res == (size_t)-1)
		{
			// could be E2BIG or EILSEQ or EINVAL
			// deal with those cases afterward if necessary
			return L"";
		}

		const size_t BytesWritten = OutBuff.size() * sizeof(wchar_t) - OutBytes;
		const size_t WCharCount = BytesWritten / sizeof(wchar_t);
		
		// output
		std::wstring WString(OutBuff.data(), WCharCount);

#else
		// just a fallback for unknown platform
		// this is not a good solution, better to identify the platform and implement corresponding conversion
		std::wstring WString(InStringA.begin(), InStringA.end());
#endif

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
				C = std::tolower(static_cast<uint8_t>(C));
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

	// generic write bool data
	void WriteBoolData(std::ofstream& FileOut, const bool& InFlag)
	{
		if (FileOut.fail())
		{
			return;
		}

		// platform-safe write of bool
		uint8_t Flag = InFlag ? 1 : 0;
		FileOut.write(reinterpret_cast<const char*>(&Flag), sizeof(Flag));
	}

	// generic read string data
	void ReadBoolData(std::ifstream& FileIn, bool& bFlag)
	{
		if (FileIn.fail())
		{
			return;
		}

		// platform-safe read of bool
		uint8_t Flag;
		FileIn.read(reinterpret_cast<char*>(&Flag), sizeof(Flag));

		bFlag = Flag > 0;
	}
}