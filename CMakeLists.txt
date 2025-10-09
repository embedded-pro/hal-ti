cmake_minimum_required(VERSION 3.24)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(HAL_TI_STANDALONE On)
endif()

option(CMAKE_COMPILE_WARNING_AS_ERROR "Enable warnings-as-error" ON)
option(HAL_TI_INCLUDE_DEFAULT_INIT "Include default initialization code; turn off when providing custom initialization" ON)
option(HAL_TI_BUILD_EXAMPLES "Enable build of the examples" OFF)
option(HAL_TI_BUILD_EXAMPLES_FREERTOS "Enable build of the FreeRTOS example" OFF)

if (HAL_TI_STANDALONE)
    set(EMIL_INCLUDE_FREERTOS ${HAL_TI_BUILD_EXAMPLES_FREERTOS})

    include(FetchContent)

    FetchContent_Declare(
        emil
        GIT_REPOSITORY https://github.com/embedded-pro/embedded-infra-lib.git
        GIT_TAG        74165ff75d8734b9968bd5f2772e542968bfe2a0 # Unreleased
    )

    FetchContent_MakeAvailable(emil)
endif()

project(hal_tiva LANGUAGES C CXX ASM VERSION 2.0.0) # x-release-please-version

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)

set_directory_properties(PROPERTY USE_FOLDERS ON)

# When building hal-ti by itself do not exclude any targets from all
# Libraries will only be excluded from all when hal-ti is a consumed as a dependency.
if (HAL_TI_STANDALONE)
    set(HAL_TI_EXCLUDE_FROM_ALL "")
else()
    set(HAL_TI_EXCLUDE_FROM_ALL "EXCLUDE_FROM_ALL")
endif()

add_subdirectory(tiva)
add_subdirectory(hal_tiva)
add_subdirectory(examples)
if (HAL_TI_STANDALONE)
    add_subdirectory(integration_test)
endif()

emil_clangformat_directories(hal_tiva DIRECTORIES .)

if (HAL_TI_STANDALONE)
    emil_folderize_all_targets()
endif()

set(CPACK_GENERATOR "ZIP;TGZ")
set(CPACK_SOURCE_IGNORE_FILES ".vs/;.git/;build/")
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION "${PROJECT_DESCRIPTION}")
set(CPACK_DEBIAN_PACKAGE_NAME "${CPACK_PACKAGE_NAME}")
set(CPACK_RPM_PACKAGE_NAME "${CPACK_PACKAGE_NAME}")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_PACKAGE_MAINTAINER "${CPACK_PACKAGE_VENDOR}")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_VENDOR}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

include(CPack)