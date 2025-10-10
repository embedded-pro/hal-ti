add_library(hal_tiva.synchronous_tiva STATIC)
emil_build_for(hal_tiva.synchronous_tiva TARGET_MCU_VENDOR ti PREREQUISITE_BOOL HAL_TI_STANDALONE)

target_include_directories(hal_tiva.synchronous_tiva PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(hal_tiva.synchronous_tiva PUBLIC
    hal.synchronous_interfaces
    hal_tiva.tiva
    services.util
)

target_sources(hal_tiva.synchronous_tiva PRIVATE
    SynchronousPwm.cpp
    SynchronousPwm.hpp
    SynchronousQuadratureEncoder.cpp
    SynchronousQuadratureEncoder.hpp
    SynchronousSpiMaster.cpp
    SynchronousSpiMaster.hpp
    SynchronousUart.cpp
    SynchronousUart.hpp
)
