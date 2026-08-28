add_subdirectory(synchronous_tiva)
add_subdirectory(tiva)
add_subdirectory(instantiations)

if (HAL_TI_INCLUDE_BRINGUP)
    add_subdirectory(bringup)
endif()
