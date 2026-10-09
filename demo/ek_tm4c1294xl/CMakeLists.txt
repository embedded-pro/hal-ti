add_executable(demo_ti.ek_tm4c1294xl Main.cpp)
emil_build_for(demo_ti.ek_tm4c1294xl TARGET_MCU tm4c1294ncpdt PREREQUISITE_BOOL HAL_TI_BUILD_EXAMPLES)

target_link_libraries(demo_ti.ek_tm4c1294xl PRIVATE
    services.util
    services.peripheral
    hal_tiva.instantiations
    hal_tiva.tiva
)

hal_ti_target_default_linker_scripts(demo_ti.ek_tm4c1294xl)
hal_ti_target_bringup(demo_ti.ek_tm4c1294xl)

emil_generate_artifacts(TARGET demo_ti.ek_tm4c1294xl LST MAP BIN HEX)
