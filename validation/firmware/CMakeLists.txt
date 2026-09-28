add_executable(hal_ti.validation_firmware)
emil_build_for(hal_ti.validation_firmware TARGET_MCU_VENDOR ti PREREQUISITE_BOOL HAL_TI_BUILD_EXAMPLES)

# The libraries build unoptimised in Debug; optimising the application keeps the image within the TM4C123's 256 KiB of flash
target_compile_options(hal_ti.validation_firmware PRIVATE -Os)

target_include_directories(hal_ti.validation_firmware PRIVATE
    boards/$<LOWER_CASE:${TARGET_MCU_FAMILY}>
)

target_link_libraries(hal_ti.validation_firmware PRIVATE
    services.util
    services.tracer
    hal_tiva.instantiations
    hal_tiva.tiva
    hal_tiva.synchronous_tiva
)

target_sources(hal_ti.validation_firmware PRIVATE
    AdcCommands.cpp
    AdcCommands.hpp
    Arguments.cpp
    Arguments.hpp
    BoardTypes.hpp
    CanCommands.cpp
    CanCommands.hpp
    ComparatorCommands.cpp
    ComparatorCommands.hpp
    Console.cpp
    Console.hpp
    Context.hpp
    EepromCommands.cpp
    EepromCommands.hpp
    $<$<STREQUAL:${TARGET_MCU_FAMILY},TM4C129>:EthernetCommands.cpp>
    $<$<NOT:$<STREQUAL:${TARGET_MCU_FAMILY},TM4C129>>:EthernetCommandsUnsupported.cpp>
    EthernetCommands.hpp
    GpioCommands.cpp
    GpioCommands.hpp
    Main.cpp
    PinPool.cpp
    PinPool.hpp
    PwmCommands.cpp
    PwmCommands.hpp
    QeiCommands.cpp
    QeiCommands.hpp
    Response.cpp
    Response.hpp
    SpiCommands.cpp
    SpiCommands.hpp
    SystemCommands.cpp
    SystemCommands.hpp
    UartCommands.cpp
    UartCommands.hpp
    WatchDogCommands.cpp
    WatchDogCommands.hpp
    $<$<STREQUAL:${TARGET_MCU_FAMILY},TM4C123>:boards/tm4c123/BoardProfile.hpp>
    $<$<STREQUAL:${TARGET_MCU_FAMILY},TM4C129>:boards/tm4c129/BoardProfile.hpp>
)

hal_ti_target_default_linker_scripts(hal_ti.validation_firmware)
hal_ti_target_bringup(hal_ti.validation_firmware)

emil_generate_artifacts(TARGET hal_ti.validation_firmware LST MAP BIN HEX)
