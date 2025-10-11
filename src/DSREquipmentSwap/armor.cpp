#include <DSREquipmentSwap/Armor.hpp>

#include <DSREquipmentSwap/Tools.hpp>

#include <Firelink/Process.h>

#include <filesystem>
#include <format>

using std::filesystem::path;

using namespace Firelink;
using namespace FirelinkDSR;
using namespace DSREquipmentSwap;

std::optional<ArmorSwap> ArmorSwapState::GetTypeSwap(const ArmorType type) const
{
    if (m_tempArmorSwaps.contains(type))
        return m_tempArmorSwaps.at(type);
    return std::nullopt;
}

void ArmorSwapState::SetTypeSwap(const int preSwapArmor, const int postSwapArmor, const ArmorType type)
{
    m_tempArmorSwaps[type] = ArmorSwap(preSwapArmor, postSwapArmor);
}

void ArmorSwapState::ClearTypeSwap(const ArmorType type)
{
    m_tempArmorSwaps.erase(type);
}

bool ArmorSwapState::HasTypeSwap(const ArmorType type) const
{
    return m_tempArmorSwaps.contains(type);
}

void ArmorSwapper::CheckArmorSwapTriggers(
    const int playerIndex,
    const DSRPlayer& player,
    const std::vector<int>& activeSpEffects,
    std::vector<SwapTrigger>& triggers,
    const ArmorType type)
{
    for (SwapTrigger& swapTrigger : triggers)
    {
        if (swapTrigger.spEffectIDTrigger > 0)
        {
            if (!contains(activeSpEffects, swapTrigger.spEffectIDTrigger))
                continue; // SpEffect not active
        }

        const int currentParamID = player.GetArmor(type);

        if (swapTrigger.paramIDTrigger > 0 && currentParamID != swapTrigger.paramIDTrigger)
            continue; // ParamID does not match

        const int newParamID = currentParamID + swapTrigger.paramIDOffset;

        if (!player.SetArmor(type, newParamID))
            Error(std::format("{} Armor ID trigger failed: {}", ArmorTypeToString.at(type), swapTrigger.ToString()));
        else
            Info(std::format("{} Armor ID trigger succeeded: {}", ArmorTypeToString.at(type), swapTrigger.ToString()));

        if (swapTrigger.spEffectIDTrigger > 0)
        {
            // Set SpEffect trigger cooldown.
            swapTrigger.ResetCooldown(playerIndex, m_triggerCooldownMs);
        }

        if (!swapTrigger.isPermanent)
        {
            // Record new to old weapon ID mapping. This may replace an existing temporary swap, which we discard.
            m_armorSwapState.SetTypeSwap(currentParamID, newParamID, type);
            Info(
                std::format(
                    "Recording temporary {} Armor swap: {} -> {}",
                    ArmorTypeToString.at(type),
                    currentParamID,
                    newParamID));
        }
    }
}

void ArmorSwapper::RevertTempArmorSwaps(const DSRPlayer& player)
{
    if (!m_armorSwapState.HasTypeSwap(ArmorType::HEAD) && !m_armorSwapState.HasTypeSwap(ArmorType::BODY)
        && !m_armorSwapState.HasTypeSwap(ArmorType::ARMS) && !m_armorSwapState.HasTypeSwap(ArmorType::LEGS))
    {
        // Report that we're forcing a revert but there are no temporary swaps to revert, for clarity.
        Info("No temporary Armor swaps to force-revert.");
    }

    for (const ArmorType type : {ArmorType::HEAD, ArmorType::BODY, ArmorType::ARMS, ArmorType::LEGS})
    {
        if (m_armorSwapState.HasTypeSwap(type))
        {
            std::optional<ArmorSwap> swap = m_armorSwapState.GetTypeSwap(type);
            Info(
                std::format(
                    "Reverting {} Armor {} to {} (forced).",
                    ArmorTypeToString.at(type),
                    swap->destArmorID,
                    swap->sourceArmorID));
            RevertTempArmorSwap(player, type);
            m_armorSwapState.ClearTypeSwap(type);
        }

        // NOTE: Armor type swaps cannot "expire" as there isn't an "active slot".
    }
}

void ArmorSwapper::RevertTempArmorSwap(const DSRPlayer& player, const ArmorType type) const
{
    std::optional<ArmorSwap> swap = m_armorSwapState.GetTypeSwap(type);
    if (!swap.has_value())
    {
        Error(std::format("Tried to revert temporary {} armor swap that does not exist.", ArmorTypeToString.at(type)));
        return;
    }

    // Check that the expected temporary weapon ID is still in the slot.
    if (player.GetArmor(type) != swap->destArmorID)
    {
        Error(
            std::format(
                "{} Armor is not the expected temporary weapon ID {}. Cannot revert swap.",
                ArmorTypeToString.at(type),
                swap->destArmorID));
        return;
    }

    if (const bool revertSuccess = player.SetArmor(type, swap->sourceArmorID); !revertSuccess)
        Error(
            std::format(
                "Failed to revert temporary {} Armor {} to {}.",
                ArmorTypeToString.at(type),
                swap->destArmorID,
                swap->sourceArmorID));
    else
        Info(
            std::format(
                "Reverted temporary {} Armor {} to {}.",
                ArmorTypeToString.at(type),
                swap->destArmorID,
                swap->sourceArmorID));
}
