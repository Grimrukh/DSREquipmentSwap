#include "SwapTrigger.h"

namespace DSREquipmentSwap
{
    int SwapTrigger::GetCooldown(const int playerIndex) const
    {
        if (playerIndex < 0 || playerIndex >= DSR_MAX_PLAYERS)
        {
            Firelink::Error(
                "Invalid player index in GetCooldown (must be 0 to " + std::to_string(DSR_MAX_PLAYERS - 1) + ").");
            return 0;
        }
        return m_playerCooldownsMs[playerIndex];
    }

    void SwapTrigger::ResetCooldown(const int playerIndex, const int cooldownMs)
    {
        if (playerIndex < 0 || playerIndex >= DSR_MAX_PLAYERS)
        {
            Firelink::Error(
                "Invalid player index in ResetCooldown (must be 0 to " + std::to_string(DSR_MAX_PLAYERS - 1) + ").");
            return;
        }
        m_playerCooldownsMs[playerIndex] = cooldownMs;
    }

    void SwapTrigger::ResetAllCooldowns(const int cooldownMs)
    {
        for (int i = 0; i < DSR_MAX_PLAYERS; ++i)
            m_playerCooldownsMs[i] = cooldownMs;
    }

    void SwapTrigger::DecrementAllCooldowns(const int decrementMs)
    {
        for (int& cooldown : m_playerCooldownsMs)
        {
            cooldown -= decrementMs;
            if (cooldown < 0)
                cooldown = 0;
        }
    }
} // DSREquipmentSwap