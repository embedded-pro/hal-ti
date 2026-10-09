add_executable(demo_ti.ek_tm4c123xgl Main.cpp)
emil_build_for(demo_ti.ek_tm4c123xgl TARGET_MCU tm4c123gh6pm PREREQUISITE_BOOL HAL_TI_BUILD_EXAMPLES)

target_link_libraries(demo_ti.ek_tm4c123xgl PRIVATE
    services.util
    services.peripheral
    hal_tiva.instantiations
    hal_tiva.tiva
)

hal_ti_target_default_linker_scripts(demo_ti.ek_tm4c123xgl)
hal_ti_target_bringup(demo_ti.ek_tm4c123xgl)

emil_generate_artifacts(TARGET demo_ti.ek_tm4c123xgl LST MAP BIN HEX)
