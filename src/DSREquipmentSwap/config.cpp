#include <DSREquipmentSwap/Config.hpp>

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
        Error(std::format("Failed to open JSON file: {}", filePath.string()));
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

    // Helper lambda for extracting SwapTriggers from JSON.
    auto extractTriggers = [&foundKeys](
                               const Json& obj,
                               const EquipmentType equipType,
                               const std::string& key,
                               std::vector<SwapTrigger>& triggerList)
    {
        if (obj.contains(key))
        {
            foundKeys.insert(key);
            int triggerCount = 0;
            for (const auto& triggerEntry : obj[key])
            {
                int spEffectIDTrigger, paramIDTrigger, paramIDOffset;
                bool isPermanent = false;
                if (triggerEntry.size() == 3)
                {
                    spEffectIDTrigger = triggerEntry[0];
                    paramIDTrigger = triggerEntry[1];
                    paramIDOffset = triggerEntry[2];
                }
                else if (triggerEntry.size() == 4)
                {
                    spEffectIDTrigger = triggerEntry[0];
                    paramIDTrigger = triggerEntry[1];
                    paramIDOffset = triggerEntry[2];
                    isPermanent = triggerEntry[3];
                }
                else
                {
                    Error(
                        std::format(
                            "Invalid swap trigger entry in '{}'. "
                            "Should be [SpEffectIDTrigger, ParamIDTrigger, ParamIDOffset, IsPermanent = false]. "
                            "IsPermanent can be omitted.",
                            key));
                    return false;
                }

                if (spEffectIDTrigger == -1 && paramIDTrigger == -1)
                {
                    Error(
                        std::format(
                            "Invalid swap trigger entry in '{}'. "
                            "At least one of SpEffectIDTrigger or ParamIDTrigger must be set to a positive value.",
                            key));
                    return false;
                }

                if (spEffectIDTrigger < -1)
                {
                    Error(
                        std::format(
                            "Invalid SpEffectIDTrigger in swap trigger entry in '{}'. Must be -1 or greater.",
                            key));
                    return false;
                }
                if (paramIDTrigger < -1)
                {
                    Error(
                        std::format(
                            "Invalid ParamIDTrigger in swap trigger entry in '{}'. Must be -1 or greater.",
                            key));
                    return false;
                }

                triggerList.emplace_back(equipType, spEffectIDTrigger, paramIDTrigger, paramIDOffset, isPermanent);
                ++triggerCount;
            }

            Info(std::format("Found {} triggers of type '{}' in JSON.", triggerCount, key));
            return true;
        }

        // trigger type omitted (no triggers)
        Info(std::format("No swap triggers of type '{}' found in JSON.", key));
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
                Error(
                    "Invalid value type for key: " + key
                    + ". Expected an integer. JSON error: " + std::string(e.what()));
                return false;
            }

            return true;
        }

        // setting key omitted (default value)
        return true;
    };

    // LEGACY: If key "LeftSpEffectTriggers" or "RightSpEffectTriggers" exists, tell user to change it to
    // "LeftWeaponSpEffectTriggers" or "RightWeaponSpEffectTriggers".
    if (jsonObject.contains("LeftSpEffectTriggers"))
    {
        Error("Legacy key 'LeftSpEffectTriggers' found in JSON. Please rename it to 'LeftWeaponSpEffectTriggers'.");
        return false;
    }
    if (jsonObject.contains("RightSpEffectTriggers"))
    {
        Error("Legacy key 'RightSpEffectTriggers' found in JSON. Please rename it to 'RightWeaponSpEffectTriggers'.");
        return false;
    }

    // Extract integer settings.
    if (!extractSetting(jsonObject, "ProcessSearchTimeoutMs", config.processSearchTimeoutMs)
        || !extractSetting(jsonObject, "ProcessSearchIntervalMs", config.processSearchIntervalMs)
        || !extractSetting(jsonObject, "MonitorIntervalMs", config.monitorIntervalMs)
        || !extractSetting(jsonObject, "GameLoadedIntervalMs", config.gameLoadedIntervalMs)
        || !extractSetting(jsonObject, "SpEffectTriggerCooldownMs", config.spEffectTriggerCooldownMs))
    {
        return false;
    }

    // Extract all swap trigger lists.
    if (!extractTriggers(jsonObject, EquipmentType::WEAPON, "LeftWeaponTriggers", config.leftWeaponTriggers)
        || !extractTriggers(jsonObject, EquipmentType::WEAPON, "RightWeaponTriggers", config.rightWeaponTriggers)
        || !extractTriggers(jsonObject, EquipmentType::ARMOR, "HeadArmorTriggers", config.headArmorTriggers)
        || !extractTriggers(jsonObject, EquipmentType::ARMOR, "BodyArmorTriggers", config.bodyArmorTriggers)
        || !extractTriggers(jsonObject, EquipmentType::RING, "ArmsArmorTriggers", config.armsArmorTriggers)
        || !extractTriggers(jsonObject, EquipmentType::RING, "LegsArmorTriggers", config.legsArmorTriggers)
        || !extractTriggers(jsonObject, EquipmentType::RING, "RingTriggers", config.ringTriggers))
    {
        return false;
    }

    // Warn if sure no other keys are present.
    for (const auto& [key, _] : jsonObject.items())
    {
        if (key == "__doc__")
            continue;  // permitted
        if (!foundKeys.contains(key))
            Warning("Ignoring unrecognized key in JSON: " + key);
    }

    // Parsing successful.
    return true;
}
