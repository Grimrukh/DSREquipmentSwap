cmake_minimum_required(VERSION 3.20)
project(DSRWeaponSwap)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(
    "${PROJECT_SOURCE_DIR}/Dependencies/Firelink/FirelinkCore"
)
add_subdirectory(
    "${PROJECT_SOURCE_DIR}/Dependencies/Firelink/FirelinkDSR"
)

add_library(DSRWeaponSwap SHARED
    src/dllmain.cpp
    src/pch.cpp
    src/DSRWeaponSwap/EquipmentSwap.cpp
    src/DSRWeaponSwap/SwapConfig.cpp
)

#add_executable(DSRWeaponSwap
#    src/main.cpp
#    src/pch.cpp
#    src/DSRWeaponSwap/EquipmentSwap.cpp
#    src/DSRWeaponSwap/SwapConfig.cpp
#)

if (WIN32)
    set_target_properties(DSRWeaponSwap PROPERTIES PREFIX "")
endif()

target_link_libraries(DSRWeaponSwap
    PUBLIC FirelinkCore
    PUBLIC FirelinkDSR
)

target_precompile_headers(DSRWeaponSwap
    PRIVATE src/pch.h
)

target_include_directories(DSRWeaponSwap
    PUBLIC include
)

# Copy runtime DLLs into the build directory.
add_custom_command(TARGET DSRWeaponSwap POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:FirelinkCore>"
    "$<TARGET_FILE_DIR:DSRWeaponSwap>"
)
add_custom_command(TARGET DSRWeaponSwap POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:FirelinkDSR>"
    "$<TARGET_FILE_DIR:DSRWeaponSwap>"
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
        COMMENT "Checking and copying default DSRWeaponSwap config if needed."
    )
else ()
    add_custom_command(
        TARGET DSRWeaponSwap POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "'DSRWeaponSwap.json' config file already exists. Not copying template."
        COMMENT "No action needed"
    )
endif()

# Install target
install(TARGETS FirelinkCore DESTINATION "${PROJECT_SOURCE_DIR}/install")
install(TARGETS FirelinkDSR DESTINATION "${PROJECT_SOURCE_DIR}/install")
install(TARGETS DSRWeaponSwap DESTINATION "${PROJECT_SOURCE_DIR}/install")
install(FILES "${USER_CONFIG_FILE}" DESTINATION "${PROJECT_SOURCE_DIR}/install")
