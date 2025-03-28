#include "pch.h"
#include <memory>

#include "Firelink/Logging.h"
#include "DSRWeaponSwap/EquipmentSwap.h"

using std::filesystem::path;
using DSRWeaponSwap::EquipmentSwapper;
using DSRWeaponSwap::EquipmentSwapperConfig;

namespace
{
    const path JSON_CONFIG_PATH = "DSRWeaponSwap.json";
    const path LOG_PATH = "DSRWeaponSwapEXE.log";
}


/// @brief Entry point for the EXE. Starts `EquipmentSwapper` main loop in a thread. Never exits the loop!
int main()
{
    // Firelink::Info("DSRWeaponSwap EXE started. Creating 'DSRWeaponSwapEXE.log' file.");
    // Firelink::SetLogFile(LOG_PATH);

    Firelink::Info("DSRWeaponSwap EXE started. Starting weapon swap trigger monitor.");

    EquipmentSwapperConfig config = EquipmentSwapper::LoadConfig(JSON_CONFIG_PATH);

    // In this executable version, we don't need a thread. We block forever here (unless the process search times out).
    const auto swapper = std::make_unique<EquipmentSwapper>(config);
    swapper->Run();

    return 0;
}
