cmake_minimum_required(VERSION 3.20)
project(DSREquipmentSwap)

set(CMAKE_CXX_STANDARD 20)

# Vendoring Firelink library sources:
add_subdirectory("Dependencies/Firelink/FirelinkCore")
add_subdirectory("Dependencies/Firelink/FirelinkDSR")

# nlohmann_json (header-only)
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json
    INTERFACE "Dependencies/nlohmann_json")

# Source:
add_subdirectory(src)

# Install Firelink:
install(TARGETS FirelinkCore)
install(TARGETS FirelinkDSR)
