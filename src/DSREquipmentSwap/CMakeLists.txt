
set(DSR_EQUIPMENT_SWAP_SOURCES
    armor.hpp
    armor.cpp
    config.hpp
    config.cpp
    equipment.hpp
    equipment.cpp
    ring.hpp
    ring.cpp
    tools.hpp
    weapon.hpp
    weapon.cpp
)

option(DSR_EQUIPMENT_SWAP_EXECUTABLE "Build DSREquipmentSwap as an executable instead of a DLL" OFF)

if(DSR_EQUIPMENT_SWAP_EXECUTABLE)
    add_executable(DSREquipmentSwap)
    target_sources(DSREquipmentSwap PRIVATE
        main.cpp
        ${DSR_EQUIPMENT_SWAP_SOURCES}
    )
else()
    # NOTE: This is not an SDK. It's a runtime hook DLL. All sources are PRIVATE.
    add_library(DSREquipmentSwap SHARED)
    target_sources(DSREquipmentSwap PRIVATE
        dllmain.cpp
        ${DSR_EQUIPMENT_SWAP_SOURCES}
    )
endif()

target_include_directories(DSREquipmentSwap PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(DSREquipmentSwap
    PUBLIC FirelinkCore FirelinkDSR nlohmann_json
)

# Copy runtime DLLs into the build directory.
add_custom_command(TARGET DSREquipmentSwap POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:FirelinkCore>"
    "$<TARGET_FILE_DIR:DSREquipmentSwap>"
)
add_custom_command(TARGET DSREquipmentSwap POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:FirelinkDSRHook>"
    "$<TARGET_FILE_DIR:DSREquipmentSwap>"
)

set(SWAP_DEFAULT_TEMPLATE_FILE "${CMAKE_CURRENT_LIST_DIR}/DSREquipmentSwapConfigTemplate.json")
set(SWAP_USER_CONFIG_FILE "${CMAKE_CURRENT_BINARY_DIR}/DSREquipmentSwap.json")

# Check if the file exists and conditionally add the copy command.
if (NOT EXISTS "${SWAP_USER_CONFIG_FILE}")
    add_custom_command(
        TARGET DSREquipmentSwap POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "DSREquipmentSwap config file missing. Copying default template..."
        COMMAND ${CMAKE_COMMAND} -E copy "${SWAP_DEFAULT_TEMPLATE_FILE}" "${SWAP_USER_CONFIG_FILE}"
        COMMENT "Checking and copying default DSREquipmentSwap config if needed."
    )
else ()
    add_custom_command(
        TARGET DSREquipmentSwap POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "'DSREquipmentSwap.json' config file already exists. Not copying template."
        COMMENT "No action needed"
    )
endif()

install(TARGETS DSREquipmentSwap FirelinkCore FirelinkDSRHook)

# Only install SWAP_USER_CONFIG_FILE to 'bin' if it doesn't already exist.
install(CODE "
    set(dst \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/DSREquipmentSwap.json\")
    if(NOT EXISTS \"\${dst}\")
        message(STATUS \"Installing default DSREquipmentSwap.json\")
        file(INSTALL DESTINATION \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin\"
             TYPE FILE
             FILES \"${SWAP_USER_CONFIG_FILE}\")
    else()
        message(STATUS \"Not installing over existing DSREquipmentSwap.json\")
    endif()
")
