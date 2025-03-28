#include <filesystem>
#include <format>
#include <memory>
#include <thread>

#include "Firelink/Logging.h"
#include "Firelink/Process.h"
#include "FirelinkDSR/DSRHook.h"
#include "FirelinkDSR/DSREnums.h"
#include "nlohmann/json.hpp"

#include "SwapConfig.h"
#include "EquipmentSwap.h"

#pragma comment(lib, "FirelinkDSR.lib")

using namespace std;
using std::filesystem::path;

using namespace Firelink;
using namespace FirelinkDSR;
using namespace DSRWeaponSwap;
using namespace DSRWeaponSwap;


EquipmentSwapper::~EquipmentSwapper()
{
    if (m_thread)
        StopThreaded();
}

void EquipmentSwapper::StartThreaded()
{
    m_thread = thread([this]{ Run(); });
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
    unique_ptr<ManagedProcess> newProcess = ManagedProcess::WaitForProcess(
        DSR_PROCESS_NAME, m_config.processSearchTimeoutMs, m_config.processSearchIntervalMs, m_stopFlag);

    if (!newProcess || m_stopFlag.load())
        return;

    // Our DSRHook is the sole owner of the process for this application.
    m_dsrHook = make_unique<DSRHook>(move(newProcess));

    // Handed dictionaries containing countdown timers for re-checking triggered SpEffect IDs.
    map<int, int> leftSpEffectTimers;
    for (const SpEffectSwapTrigger& swapTrigger : m_config.leftSpEffectTriggers)
        leftSpEffectTimers[swapTrigger.spEffectId] = 0;

    map<int, int> rightSpEffectTimers;
    for (const SpEffectSwapTrigger& swapTrigger : m_config.rightSpEffectTriggers)
        rightSpEffectTimers[swapTrigger.spEffectId] = 0;

    // Monitor triggers.
    Info("Starting Weapon/SpEffect trigger monitor loop.");
    while (true)
    {
        if (m_stopFlag.load())
            break;

        if (!ValidateHook())
            continue;  // try again (appropriate sleep already done)

        // Update temporary swaps by checking current weapons (we don't force-revert).
        CheckTempWeaponSwaps(false);

        // WEAPONS: We check and replace primary AND secondary weapons per hand.
        CheckHandedWeaponSwapTriggers(m_config.leftWeaponIdTriggers, true);
        CheckHandedWeaponSwapTriggers(m_config.rightWeaponIdTriggers, false);

        // SP EFFECT triggers: always act on CURRENT weapon in appropriate hand.
        CheckHandedSpEffectSwapTriggers(m_config.leftSpEffectTriggers, leftSpEffectTimers, true);
        CheckHandedSpEffectSwapTriggers(m_config.rightSpEffectTriggers, rightSpEffectTimers, false);

        // Update cooldown timers for SpEffect triggers.
        DecrementSpEffectTimers(leftSpEffectTimers);
        DecrementSpEffectTimers(rightSpEffectTimers);

        // Sleep for refresh interval:
        this_thread::sleep_for(chrono::milliseconds(m_config.monitorIntervalMs));
    }
}


bool EquipmentSwapper::ValidateHook()
{
    if (const shared_ptr<ManagedProcess> dsrProcess = m_dsrHook->GetProcess();
        !dsrProcess->IsHandleValid() || dsrProcess->IsProcessTerminated())
    {
        // Lost the process (invalid handle or terminated). We find a new process instance and reset the hook.

        // Release hook of stale process (will also release process if last reference).
        m_dsrHook.reset();

        // Find again with blocking call.
        Warning("Lost DSR process handle. Searching again...");
        unique_ptr<ManagedProcess> newProcess = ManagedProcess::WaitForProcess(
            DSR_PROCESS_NAME, m_config.processSearchTimeoutMs, m_config.processSearchIntervalMs, m_stopFlag);

        // Pass ownership `newProcess` to a new `DSRHook` instance (sole owner).
        m_dsrHook = make_unique<DSRHook>(move(newProcess));
    }

    // Update `m_gameLoaded` state.
    if (!m_dsrHook->IsGameLoaded())
    {
        if (m_gameLoaded)
        {
            m_gameLoaded = false;
            Warning(format("Game is not loaded. Checking again every {} ms...", m_config.gameLoadedIntervalMs));
        }
        this_thread::sleep_for(chrono::milliseconds(m_config.gameLoadedIntervalMs));
        return false;  // do not check triggers
    }

    if (!m_gameLoaded)
    {
        m_gameLoaded = true;
        // Game has been (re)-loaded. Any temporary weapon swaps need to be undone (forced revert).
        CheckTempWeaponSwaps(true);
        Info("Game is loaded. Monitoring weapon swap triggers...");
    }

    return true;
}

void EquipmentSwapper::CheckHandedWeaponSwapTriggers(
    const std::vector<WeaponIDSwapTrigger>& triggers,
    const bool isLeftHand) const
{
    for (const WeaponIDSwapTrigger& swapTrigger : triggers)
    {
        // Iterate over PRIMARY and SECONDARY slots:
        for (const WeaponSlot slot : { WeaponSlot::PRIMARY, WeaponSlot::SECONDARY })
        {
            if (const int currentWeaponId = m_dsrHook->GetWeapon(slot, isLeftHand);
                currentWeaponId == swapTrigger.weaponId)
            {
                if (!m_dsrHook->SetWeapon(slot, currentWeaponId + swapTrigger.weaponIdOffset, isLeftHand))
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

        if (m_dsrHook->PlayerHasSpEffect(swapTrigger.spEffectId))
        {
            const WeaponSlot currentSlot = m_dsrHook->GetWeaponSlot(isLeftHand);
            const int currentWeaponId = m_dsrHook->GetWeapon(currentSlot, isLeftHand);
            const int newWeaponId = currentWeaponId + swapTrigger.weaponIdOffset;

            if (!m_dsrHook->SetWeapon(currentSlot, newWeaponId, isLeftHand))
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
                Info(format("Recording temporary weapon {}-hand swap: {} -> {}",
                    isLeftHand ? "Left" : "Right", currentWeaponId, newWeaponId));
            }
        }
    }
}

void EquipmentSwapper::CheckTempWeaponSwaps(const bool forceRevert)
{
    // We need all four equipped weapon IDs on top of knowing which slot is current, so we can validate the weapon
    // before reverting it.
    const int newPrimaryLeft = m_dsrHook->GetWeapon(WeaponSlot::PRIMARY, true);
    const int newPrimaryRight = m_dsrHook->GetWeapon(WeaponSlot::PRIMARY, false);
    const int newSecondaryLeft = m_dsrHook->GetWeapon(WeaponSlot::SECONDARY, true);
    const int newSecondaryRight = m_dsrHook->GetWeapon(WeaponSlot::SECONDARY, false);
    const WeaponSlot currentLeftSlot = m_dsrHook->GetWeaponSlot(true);
    const WeaponSlot currentRightSlot = m_dsrHook->GetWeaponSlot(false);

    const int newCurrentLeft = currentLeftSlot == WeaponSlot::PRIMARY ? newPrimaryLeft : newSecondaryLeft;
    const int newCurrentRight = currentRightSlot == WeaponSlot::PRIMARY ? newPrimaryRight : newSecondaryRight;

    if (forceRevert && !m_tempSwapState.HasHandTempSwap(true) && !m_tempSwapState.HasHandTempSwap(false))
    {
        // Report that we're forcing a revert but there are no temporary swaps to revert, for clarity.
        Info("No temporary weapon swaps to forcibly revert.");
    }

    if (m_tempSwapState.HasHandTempSwap(true) && forceRevert)
    {
        Info(format("Reverting left weapon {} to {} (forced).",
                newCurrentLeft, newPrimaryLeft));
        RevertTempWeaponSwap(true);
        m_tempSwapState.ClearHandTempSwap(true);
    }
    else if (m_tempSwapState.HasHandTempSwapExpired(currentLeftSlot, true))
    {
        // Active temporary left-hand swap is no longer valid. Find and revert it.
        Info(format("Reverting left weapon {} to {} (current weapon changed to {}).",
            newCurrentLeft, newPrimaryLeft, newCurrentLeft));
        RevertTempWeaponSwap(true);
        m_tempSwapState.ClearHandTempSwap(true);
    }

    if (m_tempSwapState.HasHandTempSwap(false) && forceRevert)
    {
        Info(format("Reverting right weapon {} to {} (forced).",
                newCurrentRight, newPrimaryRight));
        RevertTempWeaponSwap(false);
        m_tempSwapState.ClearHandTempSwap(false);
    }
    else if (m_tempSwapState.HasHandTempSwapExpired(currentRightSlot, false))
    {
        // Active temporary right-hand swap is no longer valid. Find and revert it.
        Info(format("Reverting right weapon {} to {} (current weapon changed to {}).",
            newCurrentRight, newPrimaryRight, newCurrentRight));
        RevertTempWeaponSwap(false);
        m_tempSwapState.ClearHandTempSwap(false);
    }

    // Update last current slots.
    m_tempSwapState.SetLastHandSlots(currentLeftSlot, currentRightSlot);
}


void EquipmentSwapper::RevertTempWeaponSwap(const bool isLeftHand) const
{
    const string hand = isLeftHand ? "Left" : "Right";

    optional<TempSwapState::TempSwap> swap = m_tempSwapState.GetHandTempSwap(isLeftHand);
    if (!swap.has_value())
    {
        Error("Tried to revert temporary weapon swap that does not exist.");
        return;
    }

    string slotName = swap->slot == WeaponSlot::PRIMARY ? "primary" : "secondary";

    // Check that the expected temporary weapon ID is still in the slot.
    if (m_dsrHook->GetWeapon(swap->slot, isLeftHand) != swap->destWeaponId)
    {
        Error(format(
            "Weapon in {}-hand {} slot is not the expected temporary weapon ID {}. Cannot revert swap.",
            hand, slotName, swap->destWeaponId));
        return;
    }

    if (!m_dsrHook->SetWeapon(WeaponSlot::PRIMARY, swap->sourceWeaponId, isLeftHand))
        Error(format("Failed to revert {}-hand temporary {} weapon {} to {}.",
            hand, slotName, swap->destWeaponId, swap->sourceWeaponId));
    else
        Info(format("Reverted {}-hand temporary {} weapon {} to {}.",
            hand, slotName, swap->destWeaponId, swap->sourceWeaponId));
}


void EquipmentSwapper::DecrementSpEffectTimers(std::map<int, int>& timers) const
{
    // Decrement each timer countdown by monitor refresh interval:
    for (int& timer: timers | views::values)
    {
        timer -= m_config.monitorIntervalMs;
        if (timer <= 0)
            timer = 0;
    }
}


EquipmentSwapperConfig EquipmentSwapper::LoadConfig(const path& jsonConfigPath)
{
    // Load config from JSON and log settings.
    EquipmentSwapperConfig config;
    if (!ParseTriggerJson(jsonConfigPath, config))
    {
        Error(format("Failed to parse JSON file: {}", jsonConfigPath.string()));
        return {};
    }
    Info(format("Loaded settings and weapon swap triggers from file: {}", jsonConfigPath.string()));
    Info(format("Process search timeout: {} ms", config.processSearchTimeoutMs));
    Info(format("Process search interval: {} ms", config.processSearchIntervalMs));
    Info(format("Monitor interval: {} ms", config.monitorIntervalMs));
    Info(format("Game loaded interval: {} ms", config.gameLoadedIntervalMs));
    Info(format("SpEffect trigger cooldown: {} ms", config.spEffectTriggerCooldownMs));
    LogTriggers(config.leftWeaponIdTriggers, "Left-Hand Weapon ID Trigger");
    LogTriggers(config.rightWeaponIdTriggers, "Right-Hand Weapon ID Trigger");
    LogTriggers(config.leftSpEffectTriggers, "Left-Hand SpEffect ID Trigger");
    LogTriggers(config.rightSpEffectTriggers, "Right-Hand SpEffect ID Trigger");

    return config;
}
