#include <DSREquipmentSwap/Equipment.hpp>

#include <DSREquipmentSwap/Config.hpp>
#include <DSREquipmentSwap/Weapon.hpp>

#include <Firelink/Logging.h>
#include <Firelink/Pointer.h>
#include <Firelink/Process.h>
#include <FirelinkDSRHook/DSRHook.h>
#include <FirelinkDSRHook/DSRPlayer.h>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <format>
#include <memory>
#include <ranges>
#include <thread>

using std::filesystem::path;

using namespace Firelink;
using namespace FirelinkDSR;
using namespace DSREquipmentSwap;

EquipmentSwapper::~EquipmentSwapper()
{
    if (m_thread)
        StopThreaded();
}

void EquipmentSwapper::StartThreaded()
{
    m_thread = std::thread([this] { Run(); });
}

void EquipmentSwapper::StopThreaded()
{
    if (!m_thread)
        throw std::runtime_error("EquipmentSwapper thread not started. Cannot stop it.");
    m_stopFlag = true;
    m_thread->join();
}

void EquipmentSwapper::Run()
{
    // Do initial DSR process search.
    std::unique_ptr<ManagedProcess> newProcess = ManagedProcess::WaitForProcess(
        DSR_PROCESS_NAME, m_config.processSearchTimeoutMs, m_config.processSearchIntervalMs, m_stopFlag);

    if (!newProcess || m_stopFlag.load())
        return;

    // Our DSRHook is the sole owner of the managed process for this application.
    m_dsrHook = make_unique<DSRHook>(std::move(newProcess));

    m_connectedPlayers = std::vector<std::pair<int, DSRPlayer>>();
    m_connectedPlayers.reserve(DSR_MAX_PLAYERS);

    // Monitor triggers.
    Info("Starting swap trigger monitor loop.");
    while (true)
    {
        if (m_stopFlag.load())
            break;

        if (!ValidateHook())
            continue; // try again (appropriate sleep already done)

        UpdateConnectedPlayers();

        if (m_connectedPlayers.size() >= 1 && m_requestTempSwapForceRevert)
        {
            Info("Reverting weapon/armor/ring temp swaps...");
            m_requestTempSwapForceRevert = false;
            for (const auto& player : m_connectedPlayers | std::views::values)
            {
                m_weaponSwapper.CheckTempWeaponSwaps(player, true);
                m_armorSwapper.RevertTempArmorSwaps(player);
                m_ringSwapper.RevertTempRingSwaps(player);
            }
        }

        for (const auto& [playerIndex, player] : m_connectedPlayers)
        {
            // Get active SpEffects once for player.
            const std::vector<int> activeSpEffects = player.GetPlayerActiveSpEffects();

            // Update temporary swaps by checking current weapons (we don't force-revert).
            m_weaponSwapper.CheckTempWeaponSwaps(player, false);

            // WEAPONS: We check and replace primary AND secondary weapons per hand.
            m_weaponSwapper.CheckHandedSwapTriggers(
                playerIndex, player, activeSpEffects, m_config.leftWeaponTriggers, true);
            m_weaponSwapper.CheckHandedSwapTriggers(
                playerIndex, player, activeSpEffects, m_config.rightWeaponTriggers, false);

            // ARMOR
            m_armorSwapper.CheckArmorSwapTriggers(
                playerIndex, player, activeSpEffects, m_config.headArmorTriggers, ArmorType::HEAD);
            m_armorSwapper.CheckArmorSwapTriggers(
                playerIndex, player, activeSpEffects, m_config.bodyArmorTriggers, ArmorType::BODY);
            m_armorSwapper.CheckArmorSwapTriggers(
                playerIndex, player, activeSpEffects, m_config.armsArmorTriggers, ArmorType::ARMS);
            m_armorSwapper.CheckArmorSwapTriggers(
                playerIndex, player, activeSpEffects, m_config.legsArmorTriggers, ArmorType::LEGS);

            // RINGS (all slots)
            m_ringSwapper.CheckRingSwapTriggers(
                playerIndex, player, activeSpEffects, m_config.ringTriggers);

            // Decrement cooldown timers for swap triggers.
            DecrementTriggerCooldowns();
        }

        // Sleep for refresh interval:
        std::this_thread::sleep_for(std::chrono::milliseconds(m_config.monitorIntervalMs));
    }
}

bool EquipmentSwapper::ValidateHook()
{
    if (const std::shared_ptr<ManagedProcess> dsrProcess = m_dsrHook->GetProcess();
        !dsrProcess->IsHandleValid() || dsrProcess->IsProcessTerminated())
    {
        // Lost the process (invalid handle or terminated). We find a new process instance and reset the hook.

        // Release hook of stale process (will also release process if last reference).
        m_dsrHook.reset();

        // Find again with blocking call.
        Warning("Lost DSR process handle. Searching again...");
        std::unique_ptr<ManagedProcess> newProcess = ManagedProcess::WaitForProcess(
            DSR_PROCESS_NAME, m_config.processSearchTimeoutMs, m_config.processSearchIntervalMs, m_stopFlag);

        // Pass ownership `newProcess` to a new `DSRHook` instance (sole owner).
        m_dsrHook = make_unique<DSRHook>(std::move(newProcess));
    }

    // Update `m_gameLoaded` state.
    if (!m_dsrHook->IsGameLoaded())
    {
        if (m_gameLoaded)
        {
            m_gameLoaded = false;
            Warning(std::format("Game is not loaded. Checking again every {} ms...", m_config.gameLoadedIntervalMs));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(m_config.gameLoadedIntervalMs));
        return false; // do not check triggers
    }

    if (!m_gameLoaded)
    {
        m_gameLoaded = true;
        // Game has been (re)-loaded. Any temporary weapon swaps need to be undone (forced revert).
        m_requestTempSwapForceRevert = true;

        // NOTE: Connected players may not be immediately available.
        Info("Game is loaded. Monitoring equipment swap triggers...");
    }

    return true;
}

void EquipmentSwapper::UpdateConnectedPlayers()
{
    m_connectedPlayers.clear();

    const auto playerIns = m_dsrHook->PlayerIns();
    if (playerIns->IsNull())
        return; // game not loaded

    const BasePointer chrSlotArray = playerIns->ReadPointer(
        "ChrSlotArray", PLAYER_INS::CHR_INS_NO_VTABLE + CHR_INS_NO_VTABLE::CONNECTED_PLAYERS_CHR_SLOT_ARRAY);

    if (chrSlotArray.IsNull())
        return; // no connected players

    for (int i = 0; i < DSR_MAX_PLAYERS; ++i)
    {
        // Read the PlayerIns pointer for each ChrSlot (0x38 size).
        BasePointer playerInsPtr = chrSlotArray.ReadPointer("ChrSlot", i * 0x38);
        if (playerInsPtr.IsNull())
            continue; // skip if ChrSlot is null (leave as nullptr)
        m_connectedPlayers.emplace_back(i, DSRPlayer(m_dsrHook.get(), playerInsPtr));
    }
}

void EquipmentSwapper::DecrementTriggerCooldowns()
{
    // Decrement each timer countdown by monitor refresh interval:
    for (SwapTrigger& trigger : m_config.leftWeaponTriggers)
        trigger.DecrementAllCooldowns(m_config.monitorIntervalMs);
    for (SwapTrigger& trigger : m_config.rightWeaponTriggers)
        trigger.DecrementAllCooldowns(m_config.monitorIntervalMs);
    for (SwapTrigger& trigger : m_config.headArmorTriggers)
        trigger.DecrementAllCooldowns(m_config.monitorIntervalMs);
    for (SwapTrigger& trigger : m_config.bodyArmorTriggers)
        trigger.DecrementAllCooldowns(m_config.monitorIntervalMs);
    for (SwapTrigger& trigger : m_config.armsArmorTriggers)
        trigger.DecrementAllCooldowns(m_config.monitorIntervalMs);
    for (SwapTrigger& trigger : m_config.legsArmorTriggers)
        trigger.DecrementAllCooldowns(m_config.monitorIntervalMs);
    for (SwapTrigger& trigger : m_config.ringTriggers)
        trigger.DecrementAllCooldowns(m_config.monitorIntervalMs);
}

bool EquipmentSwapper::LoadConfig(const path& jsonConfigPath, EquipmentSwapperConfig& config)
{
    // Load config from JSON and log settings.
    if (!ParseTriggerJson(jsonConfigPath, config))
    {
        Error(std::format("Failed to parse JSON file: {}", jsonConfigPath.string()));
        return false;
    }
    Info(std::format("Loaded settings and weapon swap triggers from file: {}", jsonConfigPath.string()));
    Info(std::format("Process search timeout: {} ms", config.processSearchTimeoutMs));
    Info(std::format("Process search interval: {} ms", config.processSearchIntervalMs));
    Info(std::format("Monitor interval: {} ms", config.monitorIntervalMs));
    Info(std::format("Game loaded interval: {} ms", config.gameLoadedIntervalMs));
    Info(std::format("SpEffect trigger cooldown: {} ms", config.spEffectTriggerCooldownMs));
    LogTriggers(config.leftWeaponTriggers, "Left-Hand Weapon Trigger");
    LogTriggers(config.rightWeaponTriggers, "Right-Hand Weapon Trigger");
    LogTriggers(config.headArmorTriggers, "Head Armor Trigger");
    LogTriggers(config.bodyArmorTriggers, "Body Armor Trigger");
    LogTriggers(config.armsArmorTriggers, "Arms Armor Trigger");
    LogTriggers(config.legsArmorTriggers, "Legs Armor Trigger");
    LogTriggers(config.ringTriggers, "Ring Trigger");

    return true;
}