add_executable(demo_ti.tm4c1294xl_boost_drv8711)
emil_build_for(demo_ti.tm4c1294xl_boost_drv8711 TARGET_MCU tm4c1294ncpdt PREREQUISITE_BOOL HAL_TI_BUILD_EXAMPLES)

target_sources(demo_ti.tm4c1294xl_boost_drv8711 PRIVATE
    BoardPins.hpp
    Main.cpp
    StepperDemo.cpp
    StepperDemo.hpp
)

target_link_libraries(demo_ti.tm4c1294xl_boost_drv8711 PRIVATE
    drivers.motor_controller
    services.util
    services.peripheral
    hal_tiva.instantiations
    hal_tiva.synchronous_tiva
    hal_tiva.tiva
)

hal_ti_target_default_linker_scripts(demo_ti.tm4c1294xl_boost_drv8711)
hal_ti_target_bringup(demo_ti.tm4c1294xl_boost_drv8711)

emil_generate_artifacts(TARGET demo_ti.tm4c1294xl_boost_drv8711 LST MAP BIN HEX)
