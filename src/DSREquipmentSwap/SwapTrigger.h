#pragma once

#include "Config.h"

namespace DSREquipmentSwap
{

    /// @brief State manager for a single monitored swap entry.
    class SwapTrigger
    {
    public:
        explicit SwapTrigger(const SwapTriggerConfig& config)
            : m_config(config)
        {}

        /// @brief Get current cooldown for given `playerIndex`, in milliseconds.
        [[nodiscard]] int GetCooldown(int playerIndex) const;

        /// @brief Reset current cooldown for given `playerIndex` to `cooldown`.
        void ResetCooldown(int playerIndex, int cooldownMs);

        /// @brief Reset current cooldowns for all players to `cooldown`.
        void ResetAllCooldowns(int cooldownMs);

        /// @brief Decrement current cooldowns for all players by `decrementMs` milliseconds.
        void DecrementAllCooldowns(int decrementMs);

        /// @brief Get a const ref to the underlying config.
        [[nodiscard]] const SwapTriggerConfig& Config() const { return m_config; }

    private:

        // Config for swap (from JSON).
        const SwapTriggerConfig m_config;

        // Internal live usage: cooldowns for this swap.
        std::array<int, DSR_MAX_PLAYERS> m_playerCooldownsMs = {}; // per-player cooldown timers (ms)
    };

} // DSREquipmentSwap
