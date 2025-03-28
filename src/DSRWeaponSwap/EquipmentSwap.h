#pragma once

#include <atomic>
#include <filesystem>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

#include "FirelinkDSR/DSRHook.h"

#include "EquipmentSwap.h"
#include "SwapConfig.h"

namespace DSRWeaponSwap
{
    /// @brief Class that stores state used by the equipment swapper loop (`Run()`).
    class EquipmentSwapper
    {
    public:
        /// @brief Construct EquipmentSwapper with config.
        explicit EquipmentSwapper(EquipmentSwapperConfig config) : m_config(std::move(config)) {}

        /// @brief Destructor that stops the thread if it is running.
        ~EquipmentSwapper();

        /// @brief Call `Run()` in a thread and initialize `m_stopFlag` member. and return a `ThreadAndFlag`. Enable its `stopFlag` and join the thread.
        void StartThreaded();

        /// @brief Enable thread-stopping flag and join thread.
        void StopThreaded();

        /// @brief Main loop of equipment swapper.
        void Run();

        /// @brief Read and return config from JSON.
        static EquipmentSwapperConfig LoadConfig(const std::filesystem::path& jsonConfigPath);

    private:

        /// @brief
        /// Stored state about current weapon slot active in each hand, to reset temporary swaps.
        /// Whenever a hand slot changes, if a temporary swap is active in that hand, we revert the now non-current ID.
        /// We also revert any temporary swaps when the game is (re)loaded.
        /// Note that if one temporary swap overrides another in the same hand, we obviously don't revert anything.
        class TempSwapState
        {
        public:
            struct TempSwap
            {
                int sourceWeaponId;
                int destWeaponId;
                WeaponSlot slot;
            };

            [[nodiscard]] std::optional<TempSwap> GetHandTempSwap(const bool isLeftHand) const
            {
                return isLeftHand ? m_tempWeaponSwapLeft : m_tempWeaponSwapRight;
            }

            /// @brief Set (with overwrite) the temporary weapon swap for the given hand and slot.
            void SetHandTempSwap(
                const int preSwapWeapon, const int postSwapWeapon, const WeaponSlot slot, const bool isLeftHand)
            {
                auto swap = TempSwap(preSwapWeapon, postSwapWeapon, slot);
                isLeftHand ? m_tempWeaponSwapLeft = swap : m_tempWeaponSwapRight = swap;
            }

            /// @brief Clear the temporary weapon swap for the given hand, indicating that it has been reverted.
            void ClearHandTempSwap(const bool isLeftHand)
            {
                isLeftHand ? m_tempWeaponSwapLeft = std::nullopt : m_tempWeaponSwapRight = std::nullopt;
            }

            /// @brief Record the last weapon slots for each hand for future checking of temporary swap expiration.
            void SetLastHandSlots(WeaponSlot leftSlot, WeaponSlot rightSlot)
            {
                m_lastCurrentLeftWeaponSlot = leftSlot;
                m_lastCurrentRightWeaponSlot = rightSlot;
            }

            [[nodiscard]] bool HasHandTempSwapExpired(const WeaponSlot newWeaponSlot, const bool isLeftHand) const
            {
                if (isLeftHand && m_tempWeaponSwapLeft && m_tempWeaponSwapLeft->slot != newWeaponSlot)
                    return true;
                if (!isLeftHand && m_tempWeaponSwapRight && m_tempWeaponSwapRight->slot != newWeaponSlot)
                    return true;

                return false;
            }

            [[nodiscard]] bool HasHandTempSwap(const bool isLeftHand) const
            {
                if (isLeftHand)
                    return m_tempWeaponSwapLeft.has_value();
                return m_tempWeaponSwapRight.has_value();
            }

        private:
            std::optional<WeaponSlot> m_lastCurrentLeftWeaponSlot = std::nullopt;
            std::optional<WeaponSlot> m_lastCurrentRightWeaponSlot = std::nullopt;
            std::optional<TempSwap> m_tempWeaponSwapLeft = std::nullopt;
            std::optional<TempSwap> m_tempWeaponSwapRight = std::nullopt;
        };

        const EquipmentSwapperConfig m_config;
        std::optional<std::thread> m_thread = std::nullopt;
        std::atomic<bool> m_stopFlag = false;
        std::unique_ptr<FirelinkDSR::DSRHook> m_dsrHook;  // owns the process hook

        bool m_gameLoaded = true;  // assume true to start

        TempSwapState m_tempSwapState{};

        /// @brief Called on each loop update to ensure the hooked process is still valid and running.
        bool ValidateHook();

        /// @brief Process any weapon ID triggers in the given hand.
        void CheckHandedWeaponSwapTriggers(
            const std::vector<WeaponIDSwapTrigger>& triggers,
            bool isLeftHand) const;

        /// @brief Process any SpEffect ID triggers in the given hand.
        void CheckHandedSpEffectSwapTriggers(
            const std::vector<SpEffectSwapTrigger>& triggers,
            std::map<int, int>& timers,
            bool isLeftHand);

        /// @brief Update left/right hand current weapons and Undo any temporary weapon swaps that are unequipped.
        /// Temporary SpEffect-triggered weapon swaps are only maintained as long as they remain the current weapon and
        /// the game isn't unloaded.
        void CheckTempWeaponSwaps(bool forceRevert);

        /// @brief Checks that `postSwapWeapon` is equipped in the given `slot` for given hand and reverts it to
        /// `preSwapWeapon`.
        void RevertTempWeaponSwap(bool isLeftHand) const;

        /// @brief Decrement SpEffect trigger cooldown timers.
        void DecrementSpEffectTimers(std::map<int, int>& timers) const;
    };
}
