add_library(hal_tiva.default_init STATIC)
emil_build_for(hal_tiva.default_init TARGET_MCU_VENDOR ti PREREQUISITE_BOOL HAL_TI_STANDALONE)

target_include_directories(hal_tiva.default_init PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(hal_tiva.default_init PUBLIC
    hal_tiva.cortex
    hal_tiva.tiva
    ti.hal_driver
)

# Assembler does not understand -Werror
set_target_properties(hal_tiva.default_init PROPERTIES COMPILE_WARNING_AS_ERROR Off)

target_sources(hal_tiva.default_init PRIVATE
    DefaultInit.cpp
)

if (TARGET_MCU_VENDOR STREQUAL ti)
    get_target_property(startup_source ti.hal_driver_${TARGET_MCU_FAMILY} HAL_TI_STARTUP_SOURCE)

    target_sources(hal_tiva.default_init PRIVATE
        ${startup_source}
    )
endif()

function(hal_ti_target_default_init target)
    target_link_libraries(${target} PUBLIC
        $<TARGET_OBJECTS:hal_tiva.default_init>
        hal_tiva.cortex
        hal_tiva.tiva
        ti.hal_driver
    )
endfunction()
