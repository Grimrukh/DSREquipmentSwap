#include "pch.h"

#include "GrimHook/Logging.h"
#include "DSRWeaponSwap/EquipmentSwap.h"

// Entry point for the DLL
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);

            GrimHook::Info(L"DSRWeaponSwap DLL loaded. Creating 'DSRWeaponSwap.log' file.");

            // Set log file to `DSRWeaponSwap.log`.
            GrimHook::SetLogFile(L"DSRWeaponSwap.log");

            GrimHook::Info(L"DSRWeaponSwap DLL loaded. Starting weapon swap trigger monitor.");
            DSRWeaponSwap::StartSwapThread();

        break;
        case DLL_PROCESS_DETACH:
            // DLL is being unloaded. Stop swap thread.
            GrimHook::Info(L"DSRWeaponSwap DLL unloaded. Stopping weapon swap trigger monitor.");
            DSRWeaponSwap::StopSwapThread();

        break;
        default:
            break;
    }
    return TRUE;
}
