#include "config.hpp"

#include <Firelink/Logging.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <unordered_set>

using namespace Firelink;
using std::filesystem::path;

bool DSREquipmentSwap::ParseTriggerJson(const path& filePath, EquipmentSwapperConfig& config)
{
    using Json = nlohmann::json;

    // Open the JSON file.
    std::ifstream jsonFile(filePath);
    if (!jsonFile.is_open())
    {
        Error(format("Failed to open JSON file: {}", filePath.string()));
        return false;
    }

    // Parse the JSON into a nlohmann::json object.
    Json jsonObject;
    try
    {
        jsonFile >> jsonObject;
    }
    catch (const Json::parse_error& e)
    {
        Error("JSON parse error: " + std::string(e.what()));
        return false;
    }

    std::unordered_set<std::string> foundKeys = {};

    // Helper lambda for extracting Weapon ID-based triggers from JSON.
    auto extractWeaponIDTriggers = [&foundKeys](const Json& obj, const std::string& key, std::vector<WeaponIDSwapTrigger>& triggerList)
    {
        if (obj.contains(key))
        {
            foundKeys.insert(key);
            int triggerCount = 0;
            for (const auto& triggerEntry : obj[key])
            {
                if (triggerEntry.size() == 2)
                {
                    triggerList.emplace_back(triggerEntry[0], triggerEntry[1]);
                    ++triggerCount;
                }
                else
                {
                    Error(format("Invalid Weapon ID trigger entry size in '{}' (should be two elements: ID, offset).", key));
                    return false;
                }
            }

            Info(format("Found {} Weapon ID triggers of type '{}' in JSON.", triggerCount, key));
            return true;
        }

        // trigger type omitted (no triggers)
        Info(format("No Weapon ID triggers of type '{}' found in JSON.", key));
        return true;
    };

    // Helper lambda for extracting SpEffect ID-based triggers from JSON.
    auto extractSpEffectTriggers = [&foundKeys](const Json& obj, const std::string& key, std::vector<SpEffectSwapTrigger>& triggerList)
    {
        if (obj.contains(key))
        {
            foundKeys.insert(key);
            int triggerCount = 0;
            for (const auto& triggerEntry : obj[key])
            {
                if (triggerEntry.size() == 3)
                {
                    triggerList.emplace_back(triggerEntry[0], triggerEntry[1], triggerEntry[2]);
                    ++triggerCount;
                }
                else
                {
                    Error(format("Invalid SpEffect trigger entry size in '{}' (should be three elements: ID, offset, isPermanent).", key));
                    return false;
                }
            }

            Info(format("Found {} SpEffect triggers of type '{}' in JSON.", triggerCount, key));
            return true;
        }

        // trigger type omitted (no triggers)
        Info(format("No SpEffect triggers of type '{}' found in JSON.", key));
        return true;
    };

    // Helper lambda for extracting integer settings from JSON (all settings are integers, currently).
    auto extractSetting = [&foundKeys](const Json& obj, const std::string& key, int& setting)
    {
        if (obj.contains(key))
        {
            foundKeys.insert(key);
            try
            {
                setting = obj[key].get<int>();
            }
            catch (const Json::exception& e)
            {
                Error("Invalid value type for key: " + key + ". Expected an integer. JSON error: " + std::string(e.what()));
                return false;
            }

            return true;
        }

        // setting key omitted (default value)
        return true;
    };

    // Extract integer settings.
    if (!extractSetting(jsonObject, "ProcessSearchTimeoutMs", config.processSearchTimeoutMs) ||
        !extractSetting(jsonObject, "ProcessSearchIntervalMs", config.processSearchIntervalMs) ||
        !extractSetting(jsonObject, "MonitorIntervalMs", config.monitorIntervalMs) ||
        !extractSetting(jsonObject, "GameLoadedIntervalMs", config.gameLoadedIntervalMs) ||
        !extractSetting(jsonObject, "SpEffectTriggerCooldownMs", config.spEffectTriggerCooldownMs))
    {
        return false;
    }

    // Extract all four triggers lists.
    if (!extractWeaponIDTriggers(jsonObject, "LeftWeaponIDTriggers", config.leftWeaponIdTriggers) ||
        !extractWeaponIDTriggers(jsonObject, "RightWeaponIDTriggers", config.rightWeaponIdTriggers) ||
        !extractSpEffectTriggers(jsonObject, "LeftSpEffectTriggers", config.leftSpEffectTriggers) ||
        !extractSpEffectTriggers(jsonObject, "RightSpEffectTriggers", config.rightSpEffectTriggers))
    {
        return false;
    }

    // Warn if sure no other keys are present.
    for (const auto& [key, _] : jsonObject.items())
    {
        if (!foundKeys.contains(key))
            Warning("Ignoring unrecognized key in JSON: " + key);
    }

    // Parsing successful.
    return true;
}
