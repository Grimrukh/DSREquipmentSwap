#include <fstream>
#include <thread>

#include "GrimHook/logging.h"
#include "GrimHook/process.h"

#include "GrimHookDSR/DSRHook.h"
#include "GrimHookDSR/DSREnums.h"

#include "nlohmann/json.hpp"
#include "DSRWeaponSwap/EquipmentSwap.h"

#pragma comment(lib, "GrimHookDSR.lib")

using namespace std;
using namespace GrimHook;
using Json = nlohmann::json;


const wstring JSON_FILE_PATH = L"DSRWeaponSwap.json";


namespace
{
    thread SWAP_THREAD;
    atomic STOP_FLAG(false);

    typedef struct JsonConfig
    {
        int processSearchIntervalMs = 500;
        int monitorIntervalMs = 10;
        int gameLoadedIntervalMs = 200;
        int speffectTriggerCooldownMs = 500;
        vector<pair<int, int>> leftWeaponIdTriggers = {};
        vector<pair<int, int>> rightWeaponIdTriggers = {};
        vector<pair<int, int>> leftSpEffectTriggers = {};
        vector<pair<int, int>> rightSpEffectTriggers = {};
    } JsonConfig;
    
    // Function to parse JSON
    bool ParseTriggerJson(const wstring& filePath, JsonConfig& config)
    {
        // Step 1: Open the JSON file
        ifstream jsonFile(filePath.c_str());
        if (!jsonFile.is_open())
        {
            Error(L"Failed to open JSON file: " + filePath);
            return false;
        }

        // Step 2: Parse the JSON into a nlohmann::json object
        Json jsonObject;
        try
        {
            jsonFile >> jsonObject;
        }
        catch (const Json::parse_error& e)
        {
            Error("JSON parse error: " + string(e.what()));
            return false;
        }

        vector<string> foundKeys = {};

        // Step 3: Helper lambdas
        
        auto extractTriggers = [&foundKeys](const Json& obj, const string& key, vector<pair<int, int>>& triggerList)
        {
            if (obj.contains(key))
            {
                for (const auto& pair : obj[key])
                {
                    if (pair.size() == 2)
                    {
                        triggerList.emplace_back(pair[0], pair[1]);
                        foundKeys.push_back(key);
                    }
                    else
                    {
                        Error("Invalid pair size in " + key + ".");
                        return false;
                    }
                }
            }
            // trigger type omitted (no triggers)
            return true;
        };

        auto extractSetting = [&foundKeys](const Json& obj, const string& key, int& setting)
        {
            if (obj.contains(key))
            {
                try
                {
                    setting = obj[key].get<int>();  // Extract as integer
                    foundKeys.push_back(key);
                }
                catch (const Json::exception& e)
                {
                    Error("Invalid value type for key: " + key + ". Expected an integer. JSON error: " + string(e.what()));
                    return false;
                }
            }
            // key omitted (default value)
            return true;
        };

        // Step 4.1: Extract integer settings
        if (!extractSetting(jsonObject, "ProcessSearchIntervalMs", config.processSearchIntervalMs) ||
            !extractSetting(jsonObject, "MonitorIntervalMs", config.monitorIntervalMs) ||
            !extractSetting(jsonObject, "GameLoadedIntervalMs", config.gameLoadedIntervalMs) ||
            !extractSetting(jsonObject, "SpeffectTriggerCooldownMs", config.speffectTriggerCooldownMs))
        {
            return false;
        }

        // Step 4.2: Extract all four triggers
        if (!extractTriggers(jsonObject, "LeftWeaponIDTriggers", config.leftWeaponIdTriggers) ||
            !extractTriggers(jsonObject, "RightWeaponIDTriggers", config.rightWeaponIdTriggers) ||
            !extractTriggers(jsonObject, "LeftSpEffectTriggers", config.leftSpEffectTriggers) ||
            !extractTriggers(jsonObject, "RightSpEffectTriggers", config.rightSpEffectTriggers))
        {
            return false;
        }

        // Step 5: make sure no other keys are present
        for (const auto& [key, value] : jsonObject.items())
        {
            if (find(foundKeys.begin(), foundKeys.end(), key) == foundKeys.end())
            {
                Error("Unrecognized key in JSON: " + key);
                return false;
            }
        }

        return true;  // Parsing successful
    }
    
    void LogTriggers(const vector<pair<int, int>>& triggers, const wstring& label)
    {
        // Log all triggers (INFO) with the given `label`.
        
        for (const auto& [trigger, offset] : triggers)
        {
            wstring arrow = offset >= 0 ? L" -> +" : L" -> ";
            wstring msg = label;
            msg.append(L" -- ");
            msg.append(to_wstring(trigger));
            msg.append(arrow);
            msg.append(to_wstring(offset));
            Info(msg);
        }
    }

    void WaitForWindow(ManagedProcess*& process, const int refreshIntervalMs)
    {
        Info(L"Searching for process with name: " + DSRHook::processName);
    
        while (true)
        {
            if (STOP_FLAG.load())
                return;
            
            if (ManagedProcess::findProcessByName(DSRHook::processName, process))
            {
                if (process)
                {
                    Info("Process found!");
                    return;
                }
                Warning("Process not found. Retrying in " + to_string(refreshIntervalMs) + " ms...");
            }
            else
            {
                Error("Error occurred during process search.");
                break;
            }
            this_thread::sleep_for(chrono::milliseconds(refreshIntervalMs));
        }
    }

    // Returns `true` if we should continue to check ID triggers.
    bool ValidateHook(ManagedProcess*& dsrProcess, DSRHook*& dsrHook, bool& gameLoaded, const JsonConfig& config)
    {
        if (!dsrProcess->isHandleValid() || dsrProcess->isProcessTerminated())
        {
            // Lost the process.
            delete dsrHook;  // does NOT manage process handle
            delete dsrProcess;

            // Find again...
            Warning("Lost DSR process handle. Searching again...");
            while (true)
            {
                if (STOP_FLAG.load())
                    break;
                
                WaitForWindow(dsrProcess, config.processSearchIntervalMs);
                if (dsrProcess)
                    break;

                // Should not be reachable.
                Warning(L"DSR process search failed. Trying again in " + to_wstring(config.processSearchIntervalMs) + L" ms...");
                this_thread::sleep_for(chrono::milliseconds(config.processSearchIntervalMs));
            }
            dsrHook = new DSRHook(dsrProcess);
        }

        if (!dsrHook->isGameLoaded())
        {
            if (gameLoaded)
            {
                gameLoaded = false;
                Warning(L"Game is not loaded. Checking again every " + to_wstring(config.gameLoadedIntervalMs) + L" ms...");
            }
            this_thread::sleep_for(chrono::milliseconds(config.gameLoadedIntervalMs));
            return false;  // do not check triggers
        }

        if (!gameLoaded)
        {
            gameLoaded = true;
            Info("Game is loaded. Monitoring weapon swap triggers...");
        }

        return true;
    }

    void DoSwapThread()
    {
        JsonConfig config;

        if (!ParseTriggerJson(JSON_FILE_PATH, config))
        {
            Error(L"Failed to parse JSON file: " + JSON_FILE_PATH);
            return;
        }

        Info(L"Loaded settings and weapon swap triggers from " + JSON_FILE_PATH);
    
        Info(L"Process search interval: " + to_wstring(config.processSearchIntervalMs) + L" ms");
        Info(L"Monitor interval: " + to_wstring(config.monitorIntervalMs) + L" ms");
        Info(L"Game loaded interval: " + to_wstring(config.gameLoadedIntervalMs) + L" ms");
        Info(L"SpEffect trigger cooldown: " + to_wstring(config.speffectTriggerCooldownMs) + L" ms");

        LogTriggers(config.leftWeaponIdTriggers, L"Left-Hand Weapon ID Trigger");
        LogTriggers(config.rightWeaponIdTriggers, L"Right-Hand Weapon ID Trigger");
        LogTriggers(config.leftSpEffectTriggers, L"Left-Hand SpEffect ID Trigger");
        LogTriggers(config.rightSpEffectTriggers, L"Right-Hand SpEffect ID Trigger");

        ManagedProcess* dsrProcess = nullptr;

        // Do initial DSR process search.
        while (true)
        {
            if (STOP_FLAG.load())
                break;
            
            // Blocks and keeps looking for PROCESS_NAME.
            WaitForWindow(dsrProcess, config.processSearchIntervalMs);

            if (dsrProcess)
                break;

            // Should not be reachable.
            Warning(L"DSR process search failed. Trying again in " + to_wstring(config.processSearchIntervalMs) + L" ms...");
            this_thread::sleep_for(chrono::milliseconds(config.processSearchIntervalMs));
        }

        if (!dsrProcess || STOP_FLAG.load())
            return;

        auto dsrHook = new DSRHook(dsrProcess);

        // Dictionary containing countdown timers for re-checking triggered SpEffect IDs:
        map<int, int> leftSpEffectTimers;
        for (const auto& [spEffectID, offset] : config.leftSpEffectTriggers)
        {
            leftSpEffectTimers[spEffectID] = 0;
        }

        map<int, int> rightSpEffectTimers;
        for (const auto& [spEffectID, offset] : config.rightSpEffectTriggers)
        {
            rightSpEffectTimers[spEffectID] = 0;
        }

        bool gameLoaded = true;  // assume true to start

        // Monitor triggers.
        Info("Starting Weapon/SpEffect trigger monitor loop.");
        while (true)
        {
            if (STOP_FLAG.load())
                break;
            
            if (!ValidateHook(dsrProcess, dsrHook, gameLoaded, config))
                continue;  // try again (appropriate sleep already done)

            // WEAPONS: We check and replace primary AND secondary weapons per hand.

            // Left-hand:
            for (const auto& [trigger, offset] : config.leftWeaponIdTriggers)
            {
                // Iterate over PRIMARY and SECONDARY slots:
                for (WeaponSlot slot : { PRIMARY, SECONDARY })
                {
                    int currentWeaponId = dsrHook->getLeftHandWeapon(slot);
                    if (currentWeaponId == trigger)
                    {
                        int newWeaponId = currentWeaponId + offset;
                        dsrHook->setLeftHandWeapon(slot, newWeaponId);
                        Info("Left-hand weapon ID trigger: " + to_string(trigger) + " -> " + to_string(offset));
                    }
                }
            }

            // Right-hand:
            for (const auto& [trigger, offset] : config.rightWeaponIdTriggers)
            {
                // Iterate over PRIMARY and SECONDARY slots:
                for (const WeaponSlot slot : { PRIMARY, SECONDARY })
                {
                    if (const int currentWeaponId = dsrHook->getRightHandWeapon(slot); currentWeaponId == trigger)
                    {
                        const int newWeaponId = currentWeaponId + offset;
                        dsrHook->setRightHandWeapon(slot, newWeaponId);
                        Info("Right-hand weapon ID trigger: " + to_string(trigger) + " -> " + to_string(offset));
                    }
                }
            }

            // SP EFFECT triggers: always act on CURRENT weapon in appropriate hand.

            // Left-hand:
            for (const auto& [trigger, offset] : config.leftSpEffectTriggers)
            {
                if (leftSpEffectTimers[trigger] > 0)
                {
                    // Skip this trigger until timer expires:
                    continue;
                }

                if (dsrHook->playerHasSpEffect(trigger))
                {
                    Info("Left-hand SpEffect trigger: " + to_string(trigger) + " -> " + to_string(offset));
                    const int currentWeaponId = dsrHook->getLeftHandWeapon(CURRENT);
                    dsrHook->setLeftHandWeapon(CURRENT, currentWeaponId + offset);

                    // Start timer:
                    leftSpEffectTimers[trigger] = config.speffectTriggerCooldownMs;
                }
            }

            // Right-hand:
            for (const auto& [trigger, offset] : config.rightSpEffectTriggers)
            {
                if (rightSpEffectTimers[trigger] > 0)
                {
                    // Skip this trigger until timer expires:
                    continue;
                }

                if (dsrHook->playerHasSpEffect(trigger))
                {
                    Info("Right-hand SpEffect trigger: " + to_string(trigger) + " -> " + to_string(offset));
                    const int currentWeaponId = dsrHook->getRightHandWeapon(CURRENT);
                    dsrHook->setRightHandWeapon(CURRENT, currentWeaponId + offset);

                    // Start timer:
                    rightSpEffectTimers[trigger] = config.speffectTriggerCooldownMs;
                }
            }

            // Decrement each timer countdown by monitor refresh interval:
            for (auto& [spEffectID, timer] : leftSpEffectTimers)
            {
                timer -= config.monitorIntervalMs;
                if (timer <= 0)
                    timer = 0;
            }

            for (auto& [spEffectID, timer] : rightSpEffectTimers)
            {
                timer -= config.monitorIntervalMs;
                if (timer <= 0)
                    timer = 0;
            }

            // Sleep for refresh interval:
            this_thread::sleep_for(chrono::milliseconds(config.monitorIntervalMs));
        }
    }
}

void DSRWeaponSwap::StartSwapThread()
{
    // Reset stop flag.
    STOP_FLAG.store(false);
    // Run the main swap thread.
    SWAP_THREAD = thread(DoSwapThread);
}

void DSRWeaponSwap::StopSwapThread()
{
    if (SWAP_THREAD.joinable())
    {
        // Set stop flag. This is checked at the start of all `while (true)` loops in the swap monitor.
        STOP_FLAG.store(true);
        // Join the swap thread.
        SWAP_THREAD.join();
    }
}