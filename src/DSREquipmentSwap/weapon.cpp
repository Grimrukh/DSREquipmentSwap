#include <DSREquipmentSwap/Weapon.hpp>

#include <DSREquipmentSwap/Tools.hpp>

#include <Firelink/Process.h>
#include <FirelinkDSRHook/DSREnums.h>

#include <filesystem>
#include <format>

using std::filesystem::path;

using namespace Firelink;
using namespace FirelinkDSR;
using namespace DSREquipmentSwap;

std::optional<WeaponSwap> TempWeaponSwapHistory::GetHandTempSwap(const bool isLeftHand) const
{
    return isLeftHand ? m_tempWeaponSwapLeft : m_tempWeaponSwapRight;
}

void TempWeaponSwapHistory::SetHandTempSwap(
    const int preSwapWeapon, const int postSwapWeapon, const WeaponSlot slot, const bool isLeftHand)
{
    auto swap = WeaponSwap(preSwapWeapon, postSwapWeapon, slot);
    isLeftHand ? m_tempWeaponSwapLeft = swap : m_tempWeaponSwapRight = swap;
}

void TempWeaponSwapHistory::ClearHandTempSwap(const bool isLeftHand)
{
    isLeftHand ? m_tempWeaponSwapLeft = std::nullopt : m_tempWeaponSwapRight = std::nullopt;
}

void TempWeaponSwapHistory::SetLastHandSlots(WeaponSlot leftSlot, WeaponSlot rightSlot)
{
    m_lastCurrentLeftWeaponSlot = leftSlot;
    m_lastCurrentRightWeaponSlot = rightSlot;
}

bool TempWeaponSwapHistory::HasHandTempSwapExpired(const WeaponSlot newWeaponSlot, const bool isLeftHand) const
{
    if (isLeftHand && m_tempWeaponSwapLeft && m_tempWeaponSwapLeft->slot != newWeaponSlot)
        return true;
    if (!isLeftHand && m_tempWeaponSwapRight && m_tempWeaponSwapRight->slot != newWeaponSlot)
        return true;

    return false;
}

bool TempWeaponSwapHistory::HasHandTempSwap(const bool isLeftHand) const
{
    if (isLeftHand)
        return m_tempWeaponSwapLeft.has_value();
    return m_tempWeaponSwapRight.has_value();
}

void WeaponSwapper::CheckHandedSwapTriggers(
    const int playerIndex,
    const DSRPlayer& player,
    const std::vector<int>& activeSpEffects,
    std::vector<SwapTrigger>& triggers,
    const bool isLeftHand)
{
    for (SwapTrigger& swapTrigger : triggers)
    {
        if (swapTrigger.equipType != EquipmentType::WEAPON)
        {
            Error("Non-weapon trigger passed to weapon trigger checker.");
            continue;
        }

        // Iterate over PRIMARY and SECONDARY slots:
        for (const WeaponSlot slot : {WeaponSlot::PRIMARY, WeaponSlot::SECONDARY})
        {
            if (swapTrigger.spEffectIDTrigger > 0)
            {
                // Only works for current slot.
                if (slot != player.GetWeaponSlot(isLeftHand))
                    continue;
                if (!contains(activeSpEffects, swapTrigger.spEffectIDTrigger))
                    continue; // SpEffect not active
                if (swapTrigger.GetCooldown(playerIndex) > 0)
                    continue; // SpEffect trigger still on cooldown for this swap
            }

            const int currentParamID = player.GetWeapon(slot, isLeftHand);

            if (swapTrigger.paramIDTrigger > 0 && currentParamID != swapTrigger.paramIDTrigger)
                continue; // ParamID does not match

            const int newParamID = currentParamID + swapTrigger.paramIDOffset;

            if (!player.SetWeapon(slot, newParamID, isLeftHand))
                Error(
                    std::format(
                        "{}-hand weapon ID trigger failed: {}", isLeftHand ? "Left" : "Right", swapTrigger.ToString()));
            else
                Info(
                    std::format(
                        "{}-hand weapon ID trigger succeeded: {}",
                        isLeftHand ? "Left" : "Right",
                        swapTrigger.ToString()));

            if (swapTrigger.spEffectIDTrigger > 0)
            {
                // Set SpEffect trigger cooldown.
                swapTrigger.ResetCooldown(playerIndex, m_triggerCooldownMs);
            }

            if (!swapTrigger.isPermanent)
            {
                // Record new to old weapon ID mapping. This may replace an existing temporary swap, which we discard.
                m_tempWeaponSwapHistory.SetHandTempSwap(currentParamID, newParamID, slot, isLeftHand);
                Info(
                    std::format(
                        "Recording temporary weapon {}-hand swap: {} -> {}",
                        isLeftHand ? "Left" : "Right",
                        currentParamID,
                        newParamID));
            }
        }
    }
}

void WeaponSwapper::CheckTempWeaponSwaps(const DSRPlayer& player, const bool forceRevert)
{
    // We need all four equipped weapon IDs on top of knowing which slot is current, so we can validate the weapon
    // before reverting it.
    const int newPrimaryLeft = player.GetWeapon(WeaponSlot::PRIMARY, true);
    const int newPrimaryRight = player.GetWeapon(WeaponSlot::PRIMARY, false);
    const int newSecondaryLeft = player.GetWeapon(WeaponSlot::SECONDARY, true);
    const int newSecondaryRight = player.GetWeapon(WeaponSlot::SECONDARY, false);
    const WeaponSlot currentLeftSlot = player.GetWeaponSlot(true);
    const WeaponSlot currentRightSlot = player.GetWeaponSlot(false);

    const int newCurrentLeft = currentLeftSlot == WeaponSlot::PRIMARY ? newPrimaryLeft : newSecondaryLeft;
    const int newCurrentRight = currentRightSlot == WeaponSlot::PRIMARY ? newPrimaryRight : newSecondaryRight;

    if (forceRevert && !m_tempWeaponSwapHistory.HasHandTempSwap(true) && !m_tempWeaponSwapHistory.HasHandTempSwap(false))
    {
        // Report that we're forcing a revert but there are no temporary swaps to revert, for clarity.
        Info("No temporary weapon swaps to force-revert.");
    }

    if (m_tempWeaponSwapHistory.HasHandTempSwap(true) && forceRevert)
    {
        Info(std::format("Reverting left weapon {} to {} (forced).", newCurrentLeft, newPrimaryLeft));
        RevertTempWeaponSwap(player, true);
        m_tempWeaponSwapHistory.ClearHandTempSwap(true);
    }
    else if (m_tempWeaponSwapHistory.HasHandTempSwapExpired(currentLeftSlot, true))
    {
        // Active temporary left-hand swap is no longer valid. Find and revert it.
        Info(
            std::format(
                "Reverting left weapon {} to {} (current weapon changed to {}).",
                newCurrentLeft,
                newPrimaryLeft,
                newCurrentLeft));
        RevertTempWeaponSwap(player, true);
        m_tempWeaponSwapHistory.ClearHandTempSwap(true);
    }

    if (m_tempWeaponSwapHistory.HasHandTempSwap(false) && forceRevert)
    {
        Info(std::format("Reverting right weapon {} to {} (forced).", newCurrentRight, newPrimaryRight));
        RevertTempWeaponSwap(player, false);
        m_tempWeaponSwapHistory.ClearHandTempSwap(false);
    }
    else if (m_tempWeaponSwapHistory.HasHandTempSwapExpired(currentRightSlot, false))
    {
        // Active temporary right-hand swap is no longer valid. Find and revert it.
        Info(
            std::format(
                "Reverting right weapon {} to {} (current weapon changed to {}).",
                newCurrentRight,
                newPrimaryRight,
                newCurrentRight));
        RevertTempWeaponSwap(player, false);
        m_tempWeaponSwapHistory.ClearHandTempSwap(false);
    }

    // Update last current slots.
    m_tempWeaponSwapHistory.SetLastHandSlots(currentLeftSlot, currentRightSlot);
}

void WeaponSwapper::RevertTempWeaponSwap(const DSRPlayer& player, const bool isLeftHand) const
{
    const std::string hand = isLeftHand ? "Left" : "Right";

    std::optional<WeaponSwap> swap = m_tempWeaponSwapHistory.GetHandTempSwap(isLeftHand);
    if (!swap.has_value())
    {
        Error("Tried to revert temporary weapon swap that does not exist.");
        return;
    }

    std::string slotName = swap->slot == WeaponSlot::PRIMARY ? "primary" : "secondary";

    // Check that the expected temporary weapon ID is still in the slot.
    if (player.GetWeapon(swap->slot, isLeftHand) != swap->destWeaponID)
    {
        Error(
            std::format(
                "Weapon in {}-hand {} slot is not the expected temporary weapon ID {}. Cannot revert swap.",
                hand,
                slotName,
                swap->destWeaponID));
        return;
    }

    if (const bool revertSuccess = player.SetWeapon(swap->slot, swap->sourceWeaponID, isLeftHand); !revertSuccess)
        Error(
            std::format(
                "Failed to revert {}-hand temporary {} weapon {} to {}.",
                hand,
                slotName,
                swap->destWeaponID,
                swap->sourceWeaponID));
    else
        Info(
            std::format(
                "Reverted {}-hand temporary {} weapon {} to {}.",
                hand,
                slotName,
                swap->destWeaponID,
                swap->sourceWeaponID));
}
