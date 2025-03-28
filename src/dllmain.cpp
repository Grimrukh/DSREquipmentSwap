#include "pch.h"

#include "Firelink/Logging.h"
#include "DSRWeaponSwap/EquipmentSwap.h"

using std::filesystem::path;
using DSRWeaponSwap::EquipmentSwapper;
using DSRWeaponSwap::EquipmentSwapperConfig;

namespace
{
    const path JSON_CONFIG_PATH = "DSRWeaponSwap.json";
    const path LOG_PATH = "DSRWeaponSwap.log";

    std::unique_ptr<EquipmentSwapper> equipmentSwapper;
}


/// @brief Entry point for the DLL. Starts `EquipmentSwapper` main loop in a thread until library is unloaded.
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
        {
            DisableThreadLibraryCalls(hModule);

            if (equipmentSwapper != nullptr)
            {
                Firelink::Warning("DSRWeaponSwap DLL main loop has already started. Exiting...");
                break;
            }

            Firelink::Info("DSRWeaponSwap DLL loaded. Creating 'DSRWeaponSwap.log' file.");

            // Set log file to `DSRWeaponSwap.log`.
            Firelink::SetLogFile(LOG_PATH);

            Firelink::Info("DSRWeaponSwap DLL loaded. Starting weapon swap trigger monitor.");

            EquipmentSwapperConfig config = EquipmentSwapper::LoadConfig(JSON_CONFIG_PATH);
            equipmentSwapper = std::make_unique<EquipmentSwapper>(config);
            equipmentSwapper->StartThreaded();
            break;
        }

        case DLL_PROCESS_DETACH:
        {
            // DLL is being unloaded. Stop swap thread.
            if (equipmentSwapper != nullptr)
            {
                Firelink::Info("DSRWeaponSwap DLL unloading. Stopping weapon swap trigger monitor.");
                equipmentSwapper->StopThreaded();
            }
            break;
        }

        default:
            break;
    }
    return TRUE;
}
