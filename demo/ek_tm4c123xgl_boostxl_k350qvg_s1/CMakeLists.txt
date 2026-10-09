add_executable(demo_ti.ek_tm4c123xgl_boostxl_k350qvg_s1)
emil_build_for(demo_ti.ek_tm4c123xgl_boostxl_k350qvg_s1 TARGET_MCU tm4c123gh6pm PREREQUISITE_BOOL HAL_TI_BUILD_EXAMPLES)

target_sources(demo_ti.ek_tm4c123xgl_boostxl_k350qvg_s1 PRIVATE
    BoardPins.hpp
    DisplayDemo.cpp
    DisplayDemo.hpp
    Main.cpp
)

target_link_libraries(demo_ti.ek_tm4c123xgl_boostxl_k350qvg_s1 PRIVATE
    boards.boostxl_k350qvg_s1
    services.util
    services.peripheral
    hal_tiva.instantiations
    hal_tiva.tiva
)

hal_ti_target_default_linker_scripts(demo_ti.ek_tm4c123xgl_boostxl_k350qvg_s1)
hal_ti_target_bringup(demo_ti.ek_tm4c123xgl_boostxl_k350qvg_s1)

emil_generate_artifacts(TARGET demo_ti.ek_tm4c123xgl_boostxl_k350qvg_s1 LST MAP BIN HEX)
