#pragma once

#include <Firelink/Logging.h>

#include <array>
#include <filesystem>
#include <format>
#include <unordered_map>

// NOTE: Memory max is definitely less than 8 players (causes ChrSlot read errors).
#define DSR_MAX_PLAYERS 4

namespace DSREquipmentSwap
{
    enum class EquipmentType
    {
        WEAPON,
        ARMOR,
        // GOOD,
        RING,
    };

    /// @brief Map from enum to strings.
    inline const std::unordered_map<EquipmentType, std::string> EquipmentTypeToString = {
        {EquipmentType::WEAPON, "Weapon"},
        {EquipmentType::ARMOR, "Armor"},
        // { EquipmentType::GOOD, "Good" },
        {EquipmentType::RING, "Ring"},
    };

    /// @brief Swap that occurs when equipment with `paramID` is found. The equipment ID is changed by `paramIDOffset`.
    /// Must be permanent.
    struct SwapTrigger
    {
        EquipmentType equipType;
        int spEffectIDTrigger; // -1 == no SpEffect requirement
        int paramIDTrigger;    // -1 == no ParamID requirement
        int paramIDOffset;
        bool isPermanent;

        std::array<int, DSR_MAX_PLAYERS> playerCooldowns = {}; // per-player cooldown timers (ms)

        SwapTrigger(
            const EquipmentType equipType,
            const int spEffectIDTrigger,
            const int paramIDTrigger,
            const int paramIDOffset,
            const bool isPermanent)
        : equipType(equipType)
        , spEffectIDTrigger(spEffectIDTrigger)
        , paramIDTrigger(paramIDTrigger)
        , paramIDOffset(paramIDOffset)
        , isPermanent(isPermanent)
        {
            if (spEffectIDTrigger < -1)
            {
                Firelink::Error("Invalid SpEffect ID in SwapTrigger (must be -1 or greater). Setting to -1.");
                this->spEffectIDTrigger = -1;
            }
            if (paramIDTrigger < -1)
            {
                Firelink::Error("Invalid ParamID in SwapTrigger (must be -1 or greater). Setting to -1.");
                this->paramIDTrigger = -1;
            }

            for (int& cooldown : playerCooldowns)
                cooldown = 0;
        }

        [[nodiscard]] int GetCooldown(const int playerIndex) const
        {
            if (playerIndex < 0 || playerIndex >= DSR_MAX_PLAYERS)
            {
                Firelink::Error(
                    "Invalid player index in GetCooldown (must be 0 to " + std::to_string(DSR_MAX_PLAYERS - 1) + ").");
                return 0;
            }
            return playerCooldowns[playerIndex];
        }

        void ResetCooldown(const int playerIndex, const int cooldown)
        {
            if (playerIndex < 0 || playerIndex >= DSR_MAX_PLAYERS)
            {
                Firelink::Error(
                    "Invalid player index in ResetCooldown (must be 0 to " + std::to_string(DSR_MAX_PLAYERS - 1) + ").");
                return;
            }
            playerCooldowns[playerIndex] = cooldown;
        }

        void ResetAllCooldowns(const int cooldown)
        {
            for (int i = 0; i < DSR_MAX_PLAYERS; ++i)
                playerCooldowns[i] = cooldown;
        }

        void DecrementAllCooldowns(const int decrement)
        {
            for (int& cooldown : playerCooldowns)
            {
                cooldown -= decrement;
                if (cooldown < 0)
                    cooldown = 0;
            }
        }

        [[nodiscard]] std::string ToString() const
        {
            return std::format(
                "{} [SpEffect {} & ParamID {}] += {} => {}",
                EquipmentTypeToString.at(equipType),
                spEffectIDTrigger,
                paramIDTrigger,
                paramIDOffset,
                paramIDTrigger + paramIDOffset);
        }
    };

    /// @brief Holds config information for game hooking and weapon/SpEffect triggers for weapon swaps.
    struct EquipmentSwapperConfig
    {
        int processSearchTimeoutMs = 3600000; // 1 hour
        int processSearchIntervalMs = 500;
        int monitorIntervalMs = 10;
        int gameLoadedIntervalMs = 200;
        int spEffectTriggerCooldownMs = 500;

        std::vector<SwapTrigger> leftWeaponTriggers = {};
        std::vector<SwapTrigger> rightWeaponTriggers = {};
        std::vector<SwapTrigger> headArmorTriggers = {};
        std::vector<SwapTrigger> bodyArmorTriggers = {};
        std::vector<SwapTrigger> armsArmorTriggers = {};
        std::vector<SwapTrigger> legsArmorTriggers = {};
        std::vector<SwapTrigger> ringTriggers = {};
    };

    /// @brief Read a `JsonConfig` struct into `config` from the given `filePath`.
    bool ParseTriggerJson(const std::filesystem::path& filePath, EquipmentSwapperConfig& config);

    /// @brief Log all triggers (INFO) with the given `prefix`.
    inline void LogTriggers(const std::vector<SwapTrigger>& triggers, const std::string& prefix)
    {
        for (const auto& swapTrigger : triggers)
            Firelink::Info(std::format("{} -- {}", prefix, swapTrigger.ToString()));
    }
} // namespace DSREquipmentSwap
