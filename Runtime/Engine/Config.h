#pragma once
#include "../Classes/Settings.h"
#include "../../framework.h"
#include "../Classes/IniManager.h"

// UHE setting index for fast lookup
struct UHESettingIndex
{
	UHESettingIndex()
		: UHESettingIndex(UHINDEXNONE, UHINDEXNONE)
	{

	}

	UHESettingIndex(size_t InSection, size_t InKey)
		: SectionIndex(InSection)
		, KeyIndex(InKey)
	{

	}

	size_t SectionIndex;
	size_t KeyIndex;
};

class UHConfigManager
{
public:
	UHConfigManager();

	// load config
	void LoadConfig();

	// apply config
	void ApplyConfig();

	// save config
	void SaveConfig(HWND InWindow);

	// apply presentation settings
	void ApplyPresentationSettings(HWND InWindow);
	void UpdateWindowSize(HWND InWindow);

	// apply window style
	void ApplyWindowStyle(HINSTANCE InInstance, HWND InWindow);

	// toggle settings
	void ToggleTAA();
	void ToggleVsync();

	// get settings
	UHPresentationSettings& PresentationSetting();
	UHEngineSettings& EngineSetting();
	UHRenderingSettings& RenderingSetting();

	// toggle full screen flag in presentation settings
	void ToggleFullScreen();

	template<typename T>
	void GetUHESetting(const std::string& InSection, const std::string& InKey, T& OutValue)
	{
		const size_t Hash = UHUtilities::StringToHash(InSection + "." + InKey);
		const auto HashIter = UHESettingIndexTable.find(Hash);

		if (HashIter != UHESettingIndexTable.end())
		{
			const UHESettingIndex& Index = HashIter->second;
			OutValue = static_cast<T>(std::stod(UHESettings[Index.SectionIndex].KeyValue[Index.KeyIndex].ValueName));
		}
		else
		{
			for (size_t SectionIdx = 0; SectionIdx < UHESettings.size(); SectionIdx++)
			{
				if (UHESettings[SectionIdx].SectionName == InSection)
				{
					for (size_t KeyIdx = 0; KeyIdx < UHESettings[SectionIdx].KeyValue.size(); KeyIdx++)
					{
						if (UHESettings[SectionIdx].KeyValue[KeyIdx].KeyName == InKey)
						{
							OutValue = static_cast<T>(std::stod(UHESettings[SectionIdx].KeyValue[KeyIdx].ValueName));
							UHESettingIndexTable[Hash] = UHESettingIndex(SectionIdx, KeyIdx);
							return;
						}
					}
				}
			}
		}

		// no value found, the OutValue will remain the same
	}

	// string version GetUHESetting
	void GetUHESetting(const std::string& InSection, const std::string& InKey, std::string& OutValue)
	{
		const size_t Hash = UHUtilities::StringToHash(InSection + "." + InKey);
		const auto HashIter = UHESettingIndexTable.find(Hash);

		if (HashIter != UHESettingIndexTable.end())
		{
			const UHESettingIndex& Index = HashIter->second;
			OutValue = UHESettings[Index.SectionIndex].KeyValue[Index.KeyIndex].ValueName;
		}
		else
		{
			for (size_t SectionIdx = 0; SectionIdx < UHESettings.size(); SectionIdx++)
			{
				if (UHESettings[SectionIdx].SectionName == InSection)
				{
					for (size_t KeyIdx = 0; KeyIdx < UHESettings[SectionIdx].KeyValue.size(); KeyIdx++)
					{
						if (UHESettings[SectionIdx].KeyValue[KeyIdx].KeyName == InKey)
						{
							OutValue = UHESettings[SectionIdx].KeyValue[KeyIdx].ValueName;
							UHESettingIndexTable[Hash] = UHESettingIndex(SectionIdx, KeyIdx);
							return;
						}
					}
				}
			}
		}

		// no value found, the OutValue will remain the same
	}

	template<typename T>
	void SetUHESetting(const std::string& InSection, const std::string& InKey, const T& InValue)
	{
		SetUHESetting(InSection, InKey, std::to_string(InValue));
	}

	// string version SetUHESetting
	void SetUHESetting(const std::string& InSection, const std::string& InKey, const std::string& InValue)
	{
		// see if there is a chance to do caching
		const size_t Hash = UHUtilities::StringToHash(InSection + "." + InKey);
		const auto HashIter = UHESettingIndexTable.find(Hash);

		if (HashIter != UHESettingIndexTable.end())
		{
			const UHESettingIndex& Index = HashIter->second;
			UHESettings[Index.SectionIndex].KeyValue[Index.KeyIndex].ValueName = InValue;
		}
		else
		{
			for (size_t SectionIdx = 0; SectionIdx < UHESettings.size(); SectionIdx++)
			{
				if (UHESettings[SectionIdx].SectionName == InSection)
				{
					for (size_t KeyIdx = 0; KeyIdx < UHESettings[SectionIdx].KeyValue.size(); KeyIdx++)
					{
						if (UHESettings[SectionIdx].KeyValue[KeyIdx].KeyName == InKey)
						{
							UHESettings[SectionIdx].KeyValue[KeyIdx].ValueName = InValue;
							UHESettingIndexTable[Hash] = UHESettingIndex(SectionIdx, KeyIdx);
							return;
						}
					}
				}
			}
		}
	}

private:
	// presentation settings
	UHPresentationSettings PresentationSettings;

	// engine settings
	UHEngineSettings EngineSettings;

	// rendering settings
	UHRenderingSettings RenderingSettings;

	// UHE settings
	std::vector<UHIniData> UHESettings;
	std::unordered_map<size_t, UHESettingIndex> UHESettingIndexTable;
};