#pragma once

#include <DSREquipmentSwap/Config.hpp>

#include <FirelinkDSRHook/DSRPlayer.h>

#include <optional>

namespace DSREquipmentSwap
{
    /// @brief A single ring swap record.
    struct RingSwap
    {
        int sourceRingID;
        int destRingID;
    };

    /// @brief Methods and history for processing ring swaps.
    class RingSwapper
    {
    public:
        explicit RingSwapper(const int triggerCooldownMs)
        : m_triggerCooldownMs(triggerCooldownMs)
        {
        }

        /// @brief Process any ring ID triggers (all slots).
        void CheckRingSwapTriggers(
            int playerIndex,
            const FirelinkDSR::DSRPlayer& player,
            const std::vector<int>& activeSpEffects,
            std::vector<SwapTrigger>& triggers);

        /// @brief Force-revert all ring swaps. Called when the game is (re)loaded.
        void RevertTempRingSwaps(const FirelinkDSR::DSRPlayer& player);

        /// @brief Revert a single ring swap in the given slot.
        void RevertTempRingSwap(const FirelinkDSR::DSRPlayer& player, int slot) const;

    private:
        int m_triggerCooldownMs;
        std::array<std::optional<RingSwap>, 2> m_tempRingSwaps;
    };
} // namespace DSREquipmentSwap
