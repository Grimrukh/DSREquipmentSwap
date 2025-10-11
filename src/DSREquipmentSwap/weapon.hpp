#pragma once

#include <DSREquipmentSwap/Config.hpp>

#include <FirelinkDSRHook/DSRPlayer.h>

#include <optional>

namespace DSREquipmentSwap
{
    using FirelinkDSR::WeaponSlot;

    /// @brief A single weapon swap record.
    struct WeaponSwap
    {
        int sourceWeaponID;
        int destWeaponID;
        WeaponSlot slot;
    };

    /// @brief Stored state about current weapon slot active in each hand, to reset temporary swaps.
    ///
    /// @details Whenever a hand slot changes, if a temporary swap is active in that hand, we revert the now non-current
    /// ID. We also revert any temporary swaps when the game is (re)loaded. Note that if one temporary swap overrides
    /// another in the same hand, we obviously don't revert anything.
    class TempWeaponSwapHistory
    {
    public:
        [[nodiscard]] std::optional<WeaponSwap> GetHandTempSwap(bool isLeftHand) const;

        /// @brief Set (with overwrite) the temporary weapon swap for the given hand and slot.
        void SetHandTempSwap(int preSwapWeapon, int postSwapWeapon, WeaponSlot slot, bool isLeftHand);

        /// @brief Clear the temporary weapon swap for the given hand, indicating that it has been reverted.
        void ClearHandTempSwap(bool isLeftHand);

        /// @brief Record the last weapon slots for each hand for future checking of temporary swap expiration.
        void SetLastHandSlots(WeaponSlot leftSlot, WeaponSlot rightSlot);

        [[nodiscard]] bool HasHandTempSwapExpired(WeaponSlot newWeaponSlot, bool isLeftHand) const;

        [[nodiscard]] bool HasHandTempSwap(bool isLeftHand) const;

    private:
        std::optional<WeaponSlot> m_lastCurrentLeftWeaponSlot = std::nullopt;
        std::optional<WeaponSlot> m_lastCurrentRightWeaponSlot = std::nullopt;
        std::optional<WeaponSwap> m_tempWeaponSwapLeft = std::nullopt;
        std::optional<WeaponSwap> m_tempWeaponSwapRight = std::nullopt;
    };

    /// @brief Methods and history for processing weapon swaps.
    class WeaponSwapper
    {
    public:
        explicit WeaponSwapper(const int triggerCooldownMs)
        : m_triggerCooldownMs(triggerCooldownMs)
        {
        }

        /// @brief Process any weapon ID triggers in the given hand.
        void CheckHandedSwapTriggers(
            int playerIndex,
            const FirelinkDSR::DSRPlayer& player,
            const std::vector<int>& activeSpEffects,
            std::vector<SwapTrigger>& triggers,
            bool isLeftHand);

        /// @brief Update left/right hand current weapons and Undo any temporary weapon swaps that are unequipped.
        /// Temporary SpEffect-triggered weapon swaps are only maintained as long as they remain the current weapon and
        /// the game isn't unloaded.
        void CheckTempWeaponSwaps(const FirelinkDSR::DSRPlayer& player, bool forceRevert);

        /// @brief Checks that `postSwapWeapon` is equipped in the given `slot` for given hand and reverts it to
        /// `preSwapWeapon`.
        void RevertTempWeaponSwap(const FirelinkDSR::DSRPlayer& player, bool isLeftHand) const;

    private:
        int m_triggerCooldownMs;
        TempWeaponSwapHistory m_tempWeaponSwapHistory{};
    };
} // namespace DSREquipmentSwap
