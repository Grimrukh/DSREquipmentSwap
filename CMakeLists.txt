cmake_minimum_required(VERSION 3.30)
project(DSRWeaponSwap)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory("${PROJECT_SOURCE_DIR}/Dependencies/GrimHook/GrimHookCore" "${PROJECT_SOURCE_DIR}/Dependencies/GrimHook/GrimHookCore/output")
add_subdirectory("${PROJECT_SOURCE_DIR}/Dependencies/GrimHook/GrimHookDSR" "${PROJECT_SOURCE_DIR}/Dependencies/GrimHook/GrimHookDSR/output")

add_library(DSRWeaponSwap SHARED
        src/dllmain.cpp
        src/pch.cpp
        src/DSRWeaponSwap/EquipmentSwap.cpp
)
if (WIN32)
    set_target_properties(DSRWeaponSwap PROPERTIES PREFIX "")
endif()

target_link_libraries(DSRWeaponSwap
        PUBLIC GrimHookDSR
)

target_precompile_headers(DSRWeaponSwap
        PRIVATE src/pch.h
)

target_include_directories(DSRWeaponSwap
        PUBLIC include
)

# Copy default JSON file only if absent.
set(USER_CONFIG_FILE "${CMAKE_BINARY_DIR}/DSRWeaponSwap.json")
set(DEFAULT_TEMPLATE_FILE "${PROJECT_SOURCE_DIR}/DSRWeaponSwap.json")

# Check if the file exists and conditionally add the copy command.
if (NOT EXISTS "${USER_CONFIG_FILE}")
    add_custom_command(
            TARGET DSRWeaponSwap POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "DSRWeaponSwap config file missing. Copying default template..."
            COMMAND ${CMAKE_COMMAND} -E copy "${DEFAULT_TEMPLATE_FILE}" "${USER_CONFIG_FILE}"
            COMMENT "Checking and copying default DSRWeaponSwap config if needed"
    )
else ()
    add_custom_command(
            TARGET YourTargetName POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "DSRWeaponSwap config file already exists."
            COMMENT "No action needed"
    )
endif()
