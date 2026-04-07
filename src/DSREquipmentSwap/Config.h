#pragma once

#include <Firelink/Logging.h>

#include <nlohmann/json.hpp>

#include <array>
#include <filesystem>
#include <format>
#include <unordered_map>

// NOTE: Memory max is definitely less than 8 players (causes ChrSlot read errors).
#define DSR_MAX_PLAYERS 4

// If an error condition is true, log an error (inserting trigger category) and return false.
#define VALIDATE_ERROR(error_condition, message)            \
    if (error_condition)                                    \
    {                                                       \
        Firelink::Error(std::format(message, category));    \
        return false;                                       \
    }

namespace DSREquipmentSwap
{
    /// @brief Holds config information for game hooking and swap triggering.
    struct HookConfig
    {
        int processSearchTimeoutMs = 3600000; // 1 hour
        int processSearchIntervalMs = 500;
        int monitorIntervalMs = 10;
        int gameLoadedIntervalMs = 200;
        int spEffectTriggerCooldownMs = 500;
    };

    /// @brief Full JSON serialization for `GeneralSettings`.
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
        HookConfig,
        processSearchTimeoutMs,
        processSearchIntervalMs,
        monitorIntervalMs,
        gameLoadedIntervalMs,
        spEffectTriggerCooldownMs)

    /// @brief Available types of equipment (all "items").
    enum class EquipmentType
    {
        WEAPON,
        ARMOR,
        // GOOD,  // Goods cannot be swapped.
        RING,
    };

    /// @brief JSON serialization for `EquipmentType` enum.
    NLOHMANN_JSON_SERIALIZE_ENUM(
        EquipmentType,
        {
            {EquipmentType::WEAPON, "Weapon"},
            {EquipmentType::ARMOR, "Armor"},
            // {EquipmentType::GOOD, "Good"},
            {EquipmentType::RING, "Ring"},
        })

    /// @brief Map from enum to strings.
    inline const std::unordered_map<EquipmentType, std::string> EquipmentTypeToString = {
        {EquipmentType::WEAPON, "Weapon"},
        {EquipmentType::ARMOR, "Armor"},
        // { EquipmentType::GOOD, "Good" },
        {EquipmentType::RING, "Ring"},
    };

    /// @brief Swap that occurs when equipment with `paramID` is found. The equipment ID is changed by `paramIDOffset`.
    /// Must be permanent.
    struct SwapTriggerConfig
    {
        // Trigger options: exact value or range, SpEffectID and/or Param ID.
        int spEffectIDTrigger = -1;      // -1 == no SpEffect requirement
        int paramIDTrigger = -1;         // -1 == no ParamID requirement
        int maxParamIDTrigger = -1;      // -1 == not a range, paramIDTrigger is exact

        // Target Param ID. Relative by default (from whatever ID was active at trigger time).
        int targetParamID = -1;
        // If true, `targetParamID` is an absolute value, not relative to initial Param ID.
        bool isTargetIDAbsolute = false;

        // If true, swap is permanent, and will not be undone on game reload or weapon toggle.
        bool isPermanent = false;

        [[nodiscard]] bool Validate(const std::string& category) const
        {
            VALIDATE_ERROR(spEffectIDTrigger == -1 && paramIDTrigger == -1,
                "Invalid swap trigger entry in list '{}'. At least one of spEffectIDTrigger or "
                "paramIDTrigger must be set to zero or greater.");

            VALIDATE_ERROR(spEffectIDTrigger < -1,
                "Invalid spEffectIDTrigger in swap trigger entry in '{}'. Must be -1 or greater.");

            VALIDATE_ERROR(paramIDTrigger < -1,
                "Invalid paramIDTrigger in swap trigger entry in '{}'. Must be -1 or greater.");
            VALIDATE_ERROR(maxParamIDTrigger < -1,
                "Invalid maxParamIDTrigger in swap trigger entry in '{}'. Must be -1 or greater.")

            VALIDATE_ERROR(paramIDTrigger == -1 && maxParamIDTrigger != -1,
                "maxParamIDTrigger must be -1 if paramIDTrigger is -1 in '{}'.");
            VALIDATE_ERROR(paramIDTrigger != -1 && maxParamIDTrigger <= paramIDTrigger,
                "maxParamIDTrigger must be -1 or greater than paramIDTrigger in '{}'.");

            return true;
        }

        /// @brief Check if given `paramID` is a trigger.
        [[nodiscard]] bool CheckParamIDTrigger(const int paramID) const
        {
            if (paramIDTrigger <= 0)
                return false;
            if (paramID >= paramIDTrigger)
            {
                if (maxParamIDTrigger == -1 || paramIDTrigger <= maxParamIDTrigger)
                    return true;
            }
            return false;
        }

        /// @brief Compute relative or absolute target Param ID based on config and `initialParamID` at trigger time.
        [[nodiscard]] int GetTargetParamID(const int initialParamID) const
        {
            if (isTargetIDAbsolute)
                return targetParamID;
            return initialParamID + targetParamID;
        }

        /// @brief Build a string representing this swap config.
        [[nodiscard]] std::string ToString() const
        {
            std::string s;
            if (spEffectIDTrigger >= 0)
            {
                s += std::format("[SpEffect {}]", spEffectIDTrigger);
                if (paramIDTrigger >= 0)
                    s += " & ";
            }
            if (paramIDTrigger >= 0)
            {
                if (maxParamIDTrigger >= 0)
                    s += std::format("[ParamID {}-{}]", paramIDTrigger, maxParamIDTrigger);
                else
                    s += std::format("[ParamID {}]", paramIDTrigger);
            }
            if (isTargetIDAbsolute)
                s += std::format(" => {}", targetParamID);
            else if (targetParamID < 0)
                s += std::format(" -= {}", -targetParamID);  // effect target is unknown in general
            else
                s += std::format(" += {}", targetParamID);  // effect target is unknown in general
            if (isPermanent)
                s += " (Permanent)";

            return s;
        }

    };

    /// @brief JSON serialization for `SwapTriggerConfig`.
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
        SwapTriggerConfig,
        spEffectIDTrigger,
        paramIDTrigger,
        maxParamIDTrigger,
        targetParamID,
        isTargetIDAbsolute,
        isPermanent)

    /// @brief Top-level settings struct represented by JSON.
    struct EquipmentSwapConfig
    {
        HookConfig hookConfig;

        // Trigger categories:
        std::vector<SwapTriggerConfig> leftWeaponTriggers = {};
        std::vector<SwapTriggerConfig> rightWeaponTriggers = {};
        std::vector<SwapTriggerConfig> headArmorTriggers = {};
        std::vector<SwapTriggerConfig> bodyArmorTriggers = {};
        std::vector<SwapTriggerConfig> armsArmorTriggers = {};
        std::vector<SwapTriggerConfig> legsArmorTriggers = {};
        std::vector<SwapTriggerConfig> ringTriggers = {};

        [[nodiscard]] bool ValidateAll() const
        {
            bool valid = true;
            for (const auto& trigger : leftWeaponTriggers)
                valid &= trigger.Validate("leftWeaponTriggers");
            for (const auto& trigger : rightWeaponTriggers)
                valid &= trigger.Validate("rightWeaponTriggers");
            for (const auto& trigger : headArmorTriggers)
                valid &= trigger.Validate("headArmorTriggers");
            for (const auto& trigger : bodyArmorTriggers)
                valid &= trigger.Validate("bodyArmorTriggers");
            for (const auto& trigger : armsArmorTriggers)
                valid &= trigger.Validate("armsArmorTriggers");
            for (const auto& trigger : legsArmorTriggers)
                valid &= trigger.Validate("legsArmorTriggers");
            for (const auto& trigger : ringTriggers)
                valid &= trigger.Validate("ringTriggers");
            return valid;
        }
    };

    /// @brief JSON serialization for `EquipmentSwapConfig`.
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
        EquipmentSwapConfig,
        hookConfig,
        leftWeaponTriggers,
        rightWeaponTriggers,
        headArmorTriggers,
        bodyArmorTriggers,
        armsArmorTriggers,
        legsArmorTriggers,
        ringTriggers)

    /// @brief Log all triggers (INFO) with the given `prefix`.
    inline void LogTriggers(const std::vector<SwapTriggerConfig>& triggers, const std::string& prefix)
    {
        for (const auto& swapTrigger : triggers)
            Firelink::Info(std::format("{} -- {}", prefix, swapTrigger.ToString()));
    }

} // namespace DSREquipmentSwap
