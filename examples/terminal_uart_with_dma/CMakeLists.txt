add_executable(hal_tiva.terminal_uart_with_dma Main.cpp)
emil_build_for(hal_tiva.terminal_uart_with_dma TARGET_MCU_VENDOR ti PREREQUISITE_BOOL HAL_TI_BUILD_EXAMPLES)

target_link_libraries(hal_tiva.terminal_uart_with_dma PRIVATE
    services.util
    hal_tiva.instantiations
    hal_tiva.tiva
)

hal_ti_target_default_linker_scripts(hal_tiva.terminal_uart_with_dma)
hal_ti_target_default_init(hal_tiva.terminal_uart_with_dma)

emil_generate_artifacts(TARGET hal_tiva.terminal_uart_with_dma LST MAP BIN HEX)
