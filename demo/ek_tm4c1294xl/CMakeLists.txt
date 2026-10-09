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

if (TARGET hal_tiva.instantiations.lwip)
    add_executable(demo_ti.ek_tm4c1294xl_ethernet)
    emil_build_for(demo_ti.ek_tm4c1294xl_ethernet TARGET_MCU tm4c1294ncpdt PREREQUISITE_BOOL HAL_TI_BUILD_EXAMPLES)

    target_sources(demo_ti.ek_tm4c1294xl_ethernet PRIVATE
        BoardIdentity.cpp
        BoardIdentity.hpp
        EthernetDemo.cpp
        EthernetDemo.hpp
        MainEthernet.cpp
    )

    target_link_libraries(demo_ti.ek_tm4c1294xl_ethernet PRIVATE
        services.network.dns
        services.network.http
        services.util
        services.peripheral
        hal_tiva.instantiations
        hal_tiva.instantiations.lwip
        hal_tiva.tiva
    )

    hal_ti_target_default_linker_scripts(demo_ti.ek_tm4c1294xl_ethernet)
    hal_ti_target_bringup(demo_ti.ek_tm4c1294xl_ethernet)

    emil_generate_artifacts(TARGET demo_ti.ek_tm4c1294xl_ethernet LST MAP BIN HEX)
endif()
