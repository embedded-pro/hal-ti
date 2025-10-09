add_library(hal_tiva.instantiations.lwip STATIC)
emil_build_for(hal_tiva.instantiations.lwip TARGET_MCU_VENDOR ti PREREQUISITE_BOOL HAL_TI_STANDALONE)

target_include_directories(hal_tiva.instantiations.lwip PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(hal_tiva.instantiations.lwip PUBLIC
    hal_tiva.cortex
    hal_tiva.tiva
    hal_tiva.synchronous_tiva
    infra.event
    infra.timer
    services.tracer
    lwip.lwip_cpp
    services.network
)

target_sources(hal_tiva.instantiations.lwip PRIVATE
    Ethernet.hpp
    EthernetSmiObserver.cpp
    EthernetSmiObserver.hpp
)
