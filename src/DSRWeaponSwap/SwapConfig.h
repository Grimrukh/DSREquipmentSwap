#pragma once

#include <format>

namespace DSRWeaponSwap
{
    struct WeaponIDSwapTrigger
    {
        int weaponId;
        int weaponIdOffset;
        // Always permanent.

        [[nodiscard]] std::string ToString() const
        {
            return std::format("Weapon {} += {} = {}", weaponId, weaponIdOffset, weaponId + weaponIdOffset);
        }
    };

    struct SpEffectSwapTrigger
    {
        int spEffectId;
        int weaponIdOffset;
        bool isPermanent;

        [[nodiscard]] std::string ToString() const
        {
            return std::format(
                "SpEffect {} -> +{} ({})", spEffectId, weaponIdOffset, isPermanent ? "permanent" : "temporary");
        }
    };

    /// @brief Holds config information for game hooking and weapon/SpEffect triggers for weapon swaps.
    struct EquipmentSwapperConfig
    {
        int processSearchTimeoutMs = 3600000;  // 1 hour
        int processSearchIntervalMs = 500;
        int monitorIntervalMs = 10;
        int gameLoadedIntervalMs = 200;
        int spEffectTriggerCooldownMs = 500;
        std::vector<WeaponIDSwapTrigger> leftWeaponIdTriggers = {};
        std::vector<WeaponIDSwapTrigger> rightWeaponIdTriggers = {};
        std::vector<SpEffectSwapTrigger> leftSpEffectTriggers = {};
        std::vector<SpEffectSwapTrigger> rightSpEffectTriggers = {};
    };

    /// @brief Read a `JsonConfig` struct into `config` from the given `filePath`.
    bool ParseTriggerJson(const std::filesystem::path& filePath, EquipmentSwapperConfig& config);

    /// @brief Log all triggers (INFO) with the given `prefix`.
    template <typename T>
    void LogTriggers(const std::vector<T>& triggers, const std::string& prefix)
    {
        for (const T& swapTrigger : triggers)
            Firelink::Info(format("{} -- {}", prefix, swapTrigger.ToString()));
    }
}
