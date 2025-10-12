#pragma once

#include <DSREquipmentSwap/Armor.hpp>
#include <DSREquipmentSwap/Config.hpp>
#include <DSREquipmentSwap/Ring.hpp>
#include <DSREquipmentSwap/Weapon.hpp>

#include <FirelinkDSRHook/DSRHook.h>
#include <FirelinkDSRHook/DSRPlayer.h>

#include <atomic>
#include <filesystem>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

namespace DSREquipmentSwap
{
    /// @brief Class that stores state used by the equipment swapper loop (`Run()`).
    class EquipmentSwapper
    {
    public:
        /// @brief Construct EquipmentSwapper with config.
        explicit EquipmentSwapper(EquipmentSwapperConfig config)
        : m_config(std::move(config))
        , m_weaponSwapper(config.spEffectTriggerCooldownMs)
        , m_armorSwapper(config.spEffectTriggerCooldownMs)
        , m_ringSwapper(config.spEffectTriggerCooldownMs)
        {
        }

        /// @brief Destructor that stops the thread if it is running.
        ~EquipmentSwapper();

        /// @brief Call `Run()` in a thread and initialize `m_stopFlag`.
        void StartThreaded();

        /// @brief Enable thread-stopping flag and join (wait for) thread.
        void StopThreaded();

        /// @brief Main loop of equipment swapper.
        void Run();

        /// @brief Read and return config from JSON.
        static bool LoadConfig(const std::filesystem::path& jsonConfigPath, EquipmentSwapperConfig& config);

    private:
        /// @brief List of connected players (`PlayerIns` wrappers) in the game. Updated on every loop iteration.
        std::vector<std::pair<int, FirelinkDSR::DSRPlayer>> m_connectedPlayers;

        EquipmentSwapperConfig m_config;
        std::optional<std::thread> m_thread = std::nullopt;
        std::atomic<bool> m_stopFlag = false;
        std::unique_ptr<FirelinkDSR::DSRHook> m_dsrHook; // owns the process hook

        WeaponSwapper m_weaponSwapper;
        ArmorSwapper m_armorSwapper;
        RingSwapper m_ringSwapper;

        bool m_gameLoaded = true; // assume true to start
        bool m_requestTempSwapForceRevert = false; // executed when 1+ connected players are next detected

        /// @brief Called on each loop update to ensure the hooked process is still valid and running.
        bool ValidateHook();

        /// @brief Collect all connected players' `PlayerIns` pointers (up to 4).
        /// Of course, the first will point right back to the parent host PlayerIns.
        void UpdateConnectedPlayers();

        /// @brief Decrement trigger cooldown timers.
        void DecrementTriggerCooldowns();
    };
} // namespace DSREquipmentSwap
