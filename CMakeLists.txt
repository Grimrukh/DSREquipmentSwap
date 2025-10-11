
cmake_minimum_required(VERSION 3.27)
project(DSREquipmentSwap VERSION 1.6.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Vendoring Firelink library sources:
add_subdirectory(Dependencies/Firelink/FirelinkCore)
add_subdirectory(Dependencies/Firelink/FirelinkDSR)

# nlohmann_json (header-only)
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json
    INTERFACE Dependencies/nlohmann_json)

# Source:
add_subdirectory(src)

# Install Firelink:
install(TARGETS FirelinkCore)
install(TARGETS FirelinkDSR)
