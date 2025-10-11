#pragma once

#include <DSREquipmentSwap/Config.hpp>

#include <FirelinkDSRHook/DSRPlayer.h>

#include <optional>
#include <unordered_map>

namespace DSREquipmentSwap
{
    using FirelinkDSR::ArmorType;

    /// @brief A single armor swap record.
    struct ArmorSwap
    {
        int sourceArmorID;
        int destArmorID;
    };

    /// @brief Stored state about current armor in each slot, to reset temporary swaps.
    ///
    /// @details Unlike weapons, the player cannot toggle primary/secondary armor. We only need to monitor for when
    /// the game is reloaded, which reverts any temporary slots.
    /// Note that if one temporary swap overrides another in the same slot, we obviously don't revert anything.
    class ArmorSwapState
    {
    public:
        [[nodiscard]] std::optional<ArmorSwap> GetTypeSwap(ArmorType type) const;

        /// @brief Set (with overwrite) the temporary armor swap for the given slot.
        void SetTypeSwap(int preSwapArmor, int postSwapArmor, ArmorType type);

        /// @brief Clear the temporary armor swap for the given slot, indicating that it has been reverted.
        void ClearTypeSwap(ArmorType type);

        [[nodiscard]] bool HasTypeSwap(ArmorType type) const;

    private:
        std::unordered_map<ArmorType, ArmorSwap> m_tempArmorSwaps;
    };

    /// @brief Methods and history for processing armor swaps.
    class ArmorSwapper
    {
    public:
        explicit ArmorSwapper(const int triggerCooldownMs)
        : m_triggerCooldownMs(triggerCooldownMs)
        {
        }

        /// @brief Process any armor ID triggers (all slots).
        void CheckArmorSwapTriggers(
            int playerIndex,
            const FirelinkDSR::DSRPlayer& player,
            const std::vector<int>& activeSpEffects,
            std::vector<SwapTrigger>& triggers,
            ArmorType type);

        /// @brief Force-revert all armor swaps. Called when the game is (re)loaded.
        void RevertTempArmorSwaps(const FirelinkDSR::DSRPlayer& player);

        void RevertTempArmorSwap(const FirelinkDSR::DSRPlayer& player, ArmorType type) const;

    private:
        int m_triggerCooldownMs;
        // TODO: This is literally just a wrapper for `map<ArmorType, optional<ArmorSwap>>`.
        ArmorSwapState m_armorSwapState{};
    };
} // namespace DSREquipmentSwap
