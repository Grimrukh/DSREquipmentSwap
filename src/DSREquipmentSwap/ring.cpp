#include <DSREquipmentSwap/Ring.hpp>

#include <DSREquipmentSwap/Tools.hpp>

#include <Firelink/Process.h>

#include <filesystem>
#include <format>

using std::filesystem::path;

using namespace Firelink;
using namespace FirelinkDSR;
using namespace DSREquipmentSwap;

void RingSwapper::CheckRingSwapTriggers(
    const int playerIndex,
    const DSRPlayer& player,
    const std::vector<int>& activeSpEffects,
    std::vector<SwapTrigger>& triggers)
{
    for (SwapTrigger& swapTrigger : triggers)
    {
        if (swapTrigger.spEffectIDTrigger > 0)
        {
            if (!contains(activeSpEffects, swapTrigger.spEffectIDTrigger))
                continue; // SpEffect not active
        }

        // Check both ring slots.
        for (int slot = 0; slot <= 1; ++slot)
        {
            const int currentParamID = player.GetRing(slot);

            if (swapTrigger.paramIDTrigger > 0 && currentParamID != swapTrigger.paramIDTrigger)
                continue; // ParamID does not match

            const int newParamID = currentParamID + swapTrigger.paramIDOffset;

            if (!player.SetRing(slot, newParamID))
                Error(std::format("Ring ID trigger in slot {} failed: {}", slot, swapTrigger.ToString()));
            else
                Info(std::format("Ring ID trigger in slot {} succeeded: {}", slot, swapTrigger.ToString()));

            if (swapTrigger.spEffectIDTrigger > 0)
            {
                // Set SpEffect trigger cooldown.
                swapTrigger.ResetCooldown(playerIndex, m_triggerCooldownMs);
            }

            if (!swapTrigger.isPermanent)
            {
                // Record new to old ring ID mapping. This may replace an existing temporary swap, which we discard.
                m_tempRingSwaps[slot] = RingSwap(currentParamID, newParamID);
                Info(std::format("Recording temporary ring slot {} swap: {} -> {}", slot, currentParamID, newParamID));
            }
        }
    }
}

void RingSwapper::RevertTempRingSwaps(const DSRPlayer& player)
{
    if (!m_tempRingSwaps[0] && !m_tempRingSwaps[1])
    {
        // Report that we're forcing a revert but there are no temporary swaps to revert, for clarity.
        Info("No temporary Ring swaps to force-revert.");
    }

    for (int slot = 0; slot <= 1; ++slot)
    {
        if (m_tempRingSwaps[slot].has_value())
        {
            std::optional<RingSwap> swap = m_tempRingSwaps[slot];
            Info(std::format("Reverting ring slot {} to {} (forced).", slot, swap->destRingID, swap->sourceRingID));
            RevertTempRingSwap(player, slot);
            m_tempRingSwaps[slot] = std::nullopt;
        }

        // NOTE: Ring swaps cannot "expire" as there isn't an "active slot".
    }
}

void RingSwapper::RevertTempRingSwap(const DSRPlayer& player, const int slot) const
{
    std::optional<RingSwap> swap = m_tempRingSwaps[slot];
    if (!swap.has_value())
    {
        Error(std::format("Tried to revert temporary ring slot {} swap that does not exist.", slot));
        return;
    }

    // Check that the expected temporary ring ID is still in the slot.
    if (player.GetRing(slot) != swap->destRingID)
    {
        Error(
            std::format(
                "Ring slot {} is not the expected temporary ring ID {}. Cannot revert swap.", slot, swap->destRingID));
        return;
    }

    if (const bool revertSuccess = player.SetRing(slot, swap->sourceRingID); !revertSuccess)
        Error(
            std::format(
                "Failed to revert temporary ring slot {} {} to {}.", slot, swap->destRingID, swap->sourceRingID));
    else
        Info(std::format("Reverted temporary ring slot {} {} to {}.", slot, swap->destRingID, swap->sourceRingID));
}
