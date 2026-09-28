add_executable(hal_ti.validation_firmware)
emil_build_for(hal_ti.validation_firmware TARGET_MCU_VENDOR ti PREREQUISITE_BOOL HAL_TI_BUILD_EXAMPLES)

# The libraries build unoptimised in Debug; optimising the application keeps the image within the TM4C123's 256 KiB of flash
target_compile_options(hal_ti.validation_firmware PRIVATE -Os)

# The generic HIL command groups are only linked into this image, so they need the same optimisation to fit
if (NOT EMIL_HOST_BUILD)
    target_compile_options(services.hil PRIVATE -Os)
    target_compile_options(services.hil.commands PRIVATE -Os)
endif()

target_include_directories(hal_ti.validation_firmware PRIVATE
    boards/$<LOWER_CASE:${TARGET_MCU_FAMILY}>
)

target_link_libraries(hal_ti.validation_firmware PRIVATE
    services.hil
    services.hil.commands
    services.tracer
    hal_tiva.instantiations
    hal_tiva.tiva
    hal_tiva.synchronous_tiva
)

target_sources(hal_ti.validation_firmware PRIVATE
    AdcFactory.cpp
    AdcFactory.hpp
    BoardTypes.hpp
    CanFactory.cpp
    CanFactory.hpp
    ComparatorFactory.cpp
    ComparatorFactory.hpp
    Console.cpp
    Console.hpp
    EepromFactory.cpp
    EepromFactory.hpp
    $<$<STREQUAL:${TARGET_MCU_FAMILY},TM4C129>:EthernetGroup.cpp>
    $<$<NOT:$<STREQUAL:${TARGET_MCU_FAMILY},TM4C129>>:EthernetGroupUnsupported.cpp>
    EthernetGroup.hpp
    Main.cpp
    PwmFactory.cpp
    PwmFactory.hpp
    QeiFactory.cpp
    QeiFactory.hpp
    SpiFactory.cpp
    SpiFactory.hpp
    TivaBoardInfo.cpp
    TivaBoardInfo.hpp
    TivaPinFactory.cpp
    TivaPinFactory.hpp
    UartFactory.cpp
    UartFactory.hpp
    WatchDogFactory.cpp
    WatchDogFactory.hpp
    $<$<STREQUAL:${TARGET_MCU_FAMILY},TM4C123>:boards/tm4c123/BoardProfile.hpp>
    $<$<STREQUAL:${TARGET_MCU_FAMILY},TM4C129>:boards/tm4c129/BoardProfile.hpp>
)

hal_ti_target_default_linker_scripts(hal_ti.validation_firmware)
hal_ti_target_bringup(hal_ti.validation_firmware)

emil_generate_artifacts(TARGET hal_ti.validation_firmware LST MAP BIN HEX)
