# hal-ti

[![Linting & Formatting](https://github.com/embedded-pro/hal-ti/actions/workflows/linting-formatting.yml/badge.svg)](https://github.com/embedded-pro/hal-ti/actions/workflows/linting-formatting.yml)

**Description**: hal-ti is a HAL (Hardware Abstraction Layer) implementation for a range of [Texas Instruments](https://ti.com) ARM Cortex-based micro-controllers. hal-ti implements the interfaces defined as part of [embedded-infra-lib].

## Dependencies

hal-ti requires:
- [embedded-infra-lib].

## How to build the software

hal-ti cannot be built by-itself, it must be built as part of a larger project. This paragraph describes how to add hal-ti to a CMake build-system, using [embedded-infra-lib].

> CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.24)

project(MyProject VERSION 1.0.0)

include(FetchContent)

FetchContent_Declare(
    emil
    GIT_REPOSITORY https://github.com/embedded-pro/embedded-infra-lib.git
    GIT_TAG        main
)

FetchContent_Declare(
    halti
    GIT_REPOSITORY https://github.com/embedded-pro/hal-ti.git
    GIT_TAG        main
)

FetchContent_MakeAvailable(emil halti)

add_executable(myprogram Main.cpp)

target_link_libraries(myprogram PUBLIC
    infra.event
    hal_tiva.tiva
)

hal_ti_target_default_linker_scripts(myprogram)
hal_ti_target_bringup(myprogram)

```

Build options defined in the top-level `CMakeLists.txt`:

- `HAL_TI_INCLUDE_BRINGUP` (default `ON`): include the default bringup code; turn off when providing custom initialization.
- `HAL_TI_INCLUDE_LWIP` (default `OFF`, switched on by the `tm4c1294ncpdt` preset): when building hal-ti standalone, build the lwIP Ethernet instantiation (TM4C129 only).
- `HAL_TI_BUILD_TESTS` (default `OFF`): build the host unit tests (standalone builds only).
- `HAL_TI_BUILD_EXAMPLES`, `HAL_TI_BUILD_EXAMPLES_FREERTOS` (default `OFF`): build the examples (`HAL_TI_BUILD_EXAMPLES` also builds `validation/` and `demo/`).

## How to test the software

Host unit tests (GoogleTest, in `integration_test/test`) cover hardware-independent logic such as CAN bit timing and the SPI clock divisor. They are built with `-DHAL_TI_BUILD_TESTS=ON`, which the host presets set:

```bash
cmake --preset host
cmake --build --preset host-Debug
ctest --preset host
```

Driver behaviour on the peripherals themselves can only be verified in-context on the target hardware.

## Community

This project uses a [code-of-conduct](CODE_OF_CONDUCT.md) to define expected conduct in our community. Instances of abusive, harassing, or otherwise unacceptable behavior may be reported by contacting the repository maintainers.

## Contributing

Please refer to our [contributing](CONTRIBUTING.md) guide when you want to contribute to this project.

## Examples

In order to run the examples, please check the document [EK-TM4C123GXL](doc/EK-TM4C123GXL.md) (TM4C123) or [EK-TM4C1294XL](doc/EK-TM4C1294XL.md) (TM4C129) first.

The [demo](demo/README.md) folder has one firmware per board, and per board plus BoosterPack (BOOSTXL-K350QVG-S1 display, BOOST-DRV8711 stepper driver).

## License

hal-ti is licensed under the [MIT](https://choosealicense.com/licenses/mit/) [license](LICENSE) except the files and/or directories named in the [notice](NOTICE) file.

[embedded-infra-lib]: https://github.com/embedded-pro/embedded-infra-lib.git
