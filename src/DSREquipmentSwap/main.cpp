#include <DSREquipmentSwap/Config.h>
#include <DSREquipmentSwap/EquipmentSwapper.h>

#include <Firelink/Logging.h>

#include <memory>

using DSREquipmentSwap::EquipmentSwapConfig;
using DSREquipmentSwap::EquipmentSwapper;
using std::filesystem::path;

namespace
{
    const path JSON_CONFIG_PATH = "DSREquipmentSwap.json";
    const path LOG_PATH = "DSREquipmentSwapEXE.log";
} // namespace

/// @brief Entry point for the EXE. Starts `EquipmentSwapper` main loop in a thread. Never exits the loop!
int main()
{
    // DISABLED: This log grows too aggressively.
    // Firelink::Info("DSREquipmentSwap EXE started. Creating 'DSREquipmentSwapEXE.log' file.");
    // Firelink::SetLogFile(LOG_PATH);

    Firelink::Info("DSREquipmentSwap EXE started. Starting weapon swap trigger monitor.");

    EquipmentSwapConfig config;
    if (!EquipmentSwapper::LoadConfig(JSON_CONFIG_PATH, config))
    {
        Firelink::Error("Failed to load configuration. Exiting...");
        return -1;
    }

    // In this executable version, we don't need a thread. We block forever here (unless the process search times out).
    const auto swapper = std::make_unique<EquipmentSwapper>(config);
    swapper->Run();

    return 0;
}
