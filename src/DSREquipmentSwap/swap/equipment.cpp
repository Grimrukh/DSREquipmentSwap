#include "equipment.hpp"

#include "config.hpp"

#include <Firelink/Logging.h>
#include <Firelink/Pointer.h>
#include <Firelink/Process.h>
#include <FirelinkDSR/DSREnums.h>
#include <FirelinkDSR/DSRHook.h>
#include <FirelinkDSR/DSRPlayer.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <format>
#include <memory>
#include <ranges>
#include <thread>

// #pragma comment(lib, "FirelinkDSR.lib")

using std::filesystem::path;

using namespace Firelink;
using namespace FirelinkDSR;
using namespace DSREquipmentSwap;
using namespace DSREquipmentSwap;

// NOTE: Memory max is definitely less than 8 players (causes ChrSlot read errors).
#define MAX_PLAYERS 4

EquipmentSwapper::~EquipmentSwapper()
{
    if (m_thread)
        StopThreaded();
}

void EquipmentSwapper::StartThreaded()
{
    m_thread = std::thread([this]{ Run(); });
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

    m_connectedPlayers = std::vector<DSRPlayer>();
    m_connectedPlayers.reserve(MAX_PLAYERS);

    // Handed dictionaries containing countdown timers for re-checking triggered SpEffect IDs.
    std::array<std::map<int, int>, MAX_PLAYERS> leftSpEffectTimers;
    for (const SpEffectSwapTrigger& swapTrigger : m_config.leftSpEffectTriggers)
    {
        for (int i = 0; i < MAX_PLAYERS; ++i)
            leftSpEffectTimers[i][swapTrigger.spEffectId] = 0;
    }

    std::array<std::map<int, int>, MAX_PLAYERS> rightSpEffectTimers;
    for (const SpEffectSwapTrigger& swapTrigger : m_config.rightSpEffectTriggers)
    {
        for (int i = 0; i < MAX_PLAYERS; ++i)
            rightSpEffectTimers[i][swapTrigger.spEffectId] = 0;
    }

    // Monitor triggers.
    Info("Starting Weapon/SpEffect trigger monitor loop.");
    while (true)
    {
        if (m_stopFlag.load())
            break;

        if (!ValidateHook())
            continue;  // try again (appropriate sleep already done)

        UpdateConnectedPlayers();

        for (int i = 0; i < m_connectedPlayers.size(); ++i)
        {
            // If we have a valid PlayerIns pointer, set it as the host or client player instance.
            const DSRPlayer& player = m_connectedPlayers.at(i);
            std::map<int, int>& leftTimers = leftSpEffectTimers.at(i);
            std::map<int, int>& rightTimers = rightSpEffectTimers.at(i);

            // Update temporary swaps by checking current weapons (we don't force-revert).
            CheckTempWeaponSwaps(player, false);

            // WEAPONS: We check and replace primary AND secondary weapons per hand.
            CheckHandedWeaponSwapTriggers(player, m_config.leftWeaponIdTriggers, true);
            CheckHandedWeaponSwapTriggers(player, m_config.rightWeaponIdTriggers, false);

            // SP EFFECT triggers: always act on CURRENT weapon in appropriate hand.
            CheckHandedSpEffectSwapTriggers(player, m_config.leftSpEffectTriggers, leftTimers, true);
            CheckHandedSpEffectSwapTriggers(player, m_config.rightSpEffectTriggers, rightTimers, false);

            // Update cooldown timers for SpEffect triggers.
            DecrementSpEffectTimers(leftTimers);
            DecrementSpEffectTimers(rightTimers);
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
        return false;  // do not check triggers
    }

    if (!m_gameLoaded)
    {
        m_gameLoaded = true;
        // Game has been (re)-loaded. Any temporary weapon swaps need to be undone (forced revert).
        UpdateConnectedPlayers();
        for (const DSRPlayer& player : m_connectedPlayers)
            CheckTempWeaponSwaps(player, true);
        Info("Game is loaded. Monitoring weapon swap triggers...");
    }

    return true;
}

void EquipmentSwapper::UpdateConnectedPlayers()
{
    m_connectedPlayers.clear();

    const auto playerIns = m_dsrHook->PlayerIns();
    if (playerIns->IsNull())
        return;  // game not loaded

    const BasePointer chrSlotArray = playerIns->ReadPointer(
        "ChrSlotArray",
        PLAYER_INS::CHR_INS_NO_VTABLE + CHR_INS_NO_VTABLE::CONNECTED_PLAYERS_CHR_SLOT_ARRAY);

    if (chrSlotArray.IsNull())
        return;  // no connected players

    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        // Read the PlayerIns pointer for each ChrSlot (0x38 size).
        BasePointer playerInsPtr = chrSlotArray.ReadPointer("ChrSlot", i * 0x38);
        if (playerInsPtr.IsNull())
            continue;  // skip if ChrSlot is null (leave as nullptr)
        m_connectedPlayers.emplace_back(m_dsrHook.get(), playerInsPtr);
    }
}

void EquipmentSwapper::CheckHandedWeaponSwapTriggers(
    const DSRPlayer& player,
    const std::vector<WeaponIDSwapTrigger>& triggers,
    const bool isLeftHand)
{
    for (const WeaponIDSwapTrigger& swapTrigger : triggers)
    {
        // Iterate over PRIMARY and SECONDARY slots:
        for (const WeaponSlot slot : { WeaponSlot::PRIMARY, WeaponSlot::SECONDARY })
        {
            // Check if the current weapon ID matches the trigger's weapon ID.
            if (const int currentWeaponId = player.GetWeapon(slot, isLeftHand);
                currentWeaponId == swapTrigger.weaponId)
            {
                if (!player.SetWeapon(slot, currentWeaponId + swapTrigger.weaponIdOffset, isLeftHand))
                    Error(format(
                        "{}-hand weapon ID trigger failed: {}",
                        isLeftHand ? "Left" : "Right",
                        swapTrigger.ToString()));
                else
                    Info(format(
                        "{}-hand weapon ID trigger succeeded: {}",
                        isLeftHand ? "Left" : "Right",
                        swapTrigger.ToString()));
            }
        }
    }
}


void EquipmentSwapper::CheckHandedSpEffectSwapTriggers(
    const DSRPlayer& player,
    const std::vector<SpEffectSwapTrigger>& triggers,
    std::map<int, int>& timers,
    const bool isLeftHand)
{
    for (const SpEffectSwapTrigger& swapTrigger : triggers)
    {
        if (timers[swapTrigger.spEffectId] > 0)
        {
            // This SpEffect ID trigger is still on cooldown. Skip it.
            continue;
        }

        if (player.HasSpEffect(swapTrigger.spEffectId))
        {
            const WeaponSlot currentSlot = player.GetWeaponSlot(isLeftHand);
            const int currentWeaponId = player.GetWeapon(currentSlot, isLeftHand);
            const int newWeaponId = currentWeaponId + swapTrigger.weaponIdOffset;

            if (!player.SetWeapon(currentSlot, newWeaponId, isLeftHand))
            {
                Error(format(
                    "{}-hand SpEffect trigger failed: {}",
                    isLeftHand ? "Left" : "Right",
                    swapTrigger.ToString()));
                continue;
            }

            Info(format(
                "{}-hand SpEffect trigger succeeded: {}",
                isLeftHand ? "Left" : "Right",
                swapTrigger.ToString()));

            // Start timer.
            timers[swapTrigger.spEffectId] = m_config.spEffectTriggerCooldownMs;

            if (!swapTrigger.isPermanent)
            {
                // Record new to old weapon ID mapping. This may replace an existing temporary swap, which we discard.
                m_tempSwapState.SetHandTempSwap(currentWeaponId, newWeaponId, currentSlot, isLeftHand);
                Info(std::format("Recording temporary weapon {}-hand swap: {} -> {}",
                    isLeftHand ? "Left" : "Right", currentWeaponId, newWeaponId));
            }
        }
    }
}

void EquipmentSwapper::CheckTempWeaponSwaps(
    const DSRPlayer& player,
    const bool forceRevert)
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

    if (forceRevert && !m_tempSwapState.HasHandTempSwap(true) && !m_tempSwapState.HasHandTempSwap(false))
    {
        // Report that we're forcing a revert but there are no temporary swaps to revert, for clarity.
        Info("No temporary weapon swaps to forcibly revert.");
    }

    if (m_tempSwapState.HasHandTempSwap(true) && forceRevert)
    {
        Info(std::format("Reverting left weapon {} to {} (forced).",
                newCurrentLeft, newPrimaryLeft));
        RevertTempWeaponSwap(player, true);
        m_tempSwapState.ClearHandTempSwap(true);
    }
    else if (m_tempSwapState.HasHandTempSwapExpired(currentLeftSlot, true))
    {
        // Active temporary left-hand swap is no longer valid. Find and revert it.
        Info(std::format("Reverting left weapon {} to {} (current weapon changed to {}).",
            newCurrentLeft, newPrimaryLeft, newCurrentLeft));
        RevertTempWeaponSwap(player, true);
        m_tempSwapState.ClearHandTempSwap(true);
    }

    if (m_tempSwapState.HasHandTempSwap(false) && forceRevert)
    {
        Info(std::format("Reverting right weapon {} to {} (forced).",
                newCurrentRight, newPrimaryRight));
        RevertTempWeaponSwap(player, false);
        m_tempSwapState.ClearHandTempSwap(false);
    }
    else if (m_tempSwapState.HasHandTempSwapExpired(currentRightSlot, false))
    {
        // Active temporary right-hand swap is no longer valid. Find and revert it.
        Info(std::format("Reverting right weapon {} to {} (current weapon changed to {}).",
            newCurrentRight, newPrimaryRight, newCurrentRight));
        RevertTempWeaponSwap(player, false);
        m_tempSwapState.ClearHandTempSwap(false);
    }

    // Update last current slots.
    m_tempSwapState.SetLastHandSlots(currentLeftSlot, currentRightSlot);
}


void EquipmentSwapper::RevertTempWeaponSwap(const DSRPlayer& player, const bool isLeftHand) const
{
    const std::string hand = isLeftHand ? "Left" : "Right";

    std::optional<TempSwapState::TempSwap> swap = m_tempSwapState.GetHandTempSwap(isLeftHand);
    if (!swap.has_value())
    {
        Error("Tried to revert temporary weapon swap that does not exist.");
        return;
    }

    std::string slotName = swap->slot == WeaponSlot::PRIMARY ? "primary" : "secondary";

    // Check that the expected temporary weapon ID is still in the slot.
    if (player.GetWeapon(swap->slot, isLeftHand) != swap->destWeaponId)
    {
        Error(format(
            "Weapon in {}-hand {} slot is not the expected temporary weapon ID {}. Cannot revert swap.",
            hand, slotName, swap->destWeaponId));
        return;
    }

    if (const bool revertSuccess = player.SetWeapon(swap->slot, swap->sourceWeaponId, isLeftHand); !revertSuccess)
        Error(format("Failed to revert {}-hand temporary {} weapon {} to {}.",
            hand, slotName, swap->destWeaponId, swap->sourceWeaponId));
    else
        Info(format("Reverted {}-hand temporary {} weapon {} to {}.",
            hand, slotName, swap->destWeaponId, swap->sourceWeaponId));
}


void EquipmentSwapper::DecrementSpEffectTimers(std::map<int, int>& timers) const
{
    // Decrement each timer countdown by monitor refresh interval:
    for (int& timer: timers | std::views::values)
    {
        timer -= m_config.monitorIntervalMs;
        if (timer <= 0)
            timer = 0;
    }
}


bool EquipmentSwapper::LoadConfig(const path& jsonConfigPath, EquipmentSwapperConfig& config)
{
    // Load config from JSON and log settings.
    if (!ParseTriggerJson(jsonConfigPath, config))
    {
        Error(format("Failed to parse JSON file: {}", jsonConfigPath.string()));
        return false;
    }
    Info(std::format("Loaded settings and weapon swap triggers from file: {}", jsonConfigPath.string()));
    Info(std::format("Process search timeout: {} ms", config.processSearchTimeoutMs));
    Info(std::format("Process search interval: {} ms", config.processSearchIntervalMs));
    Info(std::format("Monitor interval: {} ms", config.monitorIntervalMs));
    Info(std::format("Game loaded interval: {} ms", config.gameLoadedIntervalMs));
    Info(std::format("SpEffect trigger cooldown: {} ms", config.spEffectTriggerCooldownMs));
    LogTriggers(config.leftWeaponIdTriggers, "Left-Hand Weapon ID Trigger");
    LogTriggers(config.rightWeaponIdTriggers, "Right-Hand Weapon ID Trigger");
    LogTriggers(config.leftSpEffectTriggers, "Left-Hand SpEffect ID Trigger");
    LogTriggers(config.rightSpEffectTriggers, "Right-Hand SpEffect ID Trigger");

    return true;
}
