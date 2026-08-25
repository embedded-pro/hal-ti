add_library(hal_tiva.cortex STATIC)
emil_build_for(hal_tiva.cortex TARGET_MCU_VENDOR ti PREREQUISITE_BOOL HAL_TI_STANDALONE)

target_include_directories(hal_tiva.cortex PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(hal_tiva.cortex PUBLIC
    hal.cortex_m
    hal.interfaces
    infra.event
    infra.timer
    ti.hal_driver
)

target_sources(hal_tiva.cortex PRIVATE
    Reset.cpp
    Reset.hpp
    SystemTick.cpp
    SystemTick.hpp
    SystemTickTimerService.cpp
    SystemTickTimerService.hpp
    TimeKeeper.cpp
    TimeKeeper.hpp
)

add_subdirectory(synchronous_tiva)
add_subdirectory(tiva)
add_subdirectory(instantiations)
add_subdirectory(bringup)
