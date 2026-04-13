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
cmake_minimum_required(VERSION 3.21)

project(MyProject VERSION 1.0.0)

include(FetchContent)

FetchContent_Declare(
    emil
    GIT_REPOSITORY https://github.com/embedded-pro/embedded-infra-lib.git
    GIT_TAG        main
)

FetchContent_Declare(
    halst
    GIT_REPOSITORY https://github.com/embedded-pro/hal-ti.git
    GIT_TAG        main
)

FetchContent_MakeAvailable(emil halst)

add_executable(myprogram Main.cpp)

target_link_libraries(myprogram PUBLIC
    infra.event
    hal_st.tiva
)

hal_ti_target_default_linker_scripts(myprogram)
hal_ti_target_default_init(myprogram)

```

## How to test the software

Due to the nature of hal-ti: a HAL implementation, there are no included automated tests. Testing should be done in-context on the target hardware.

## Community

This project uses a [code-of-conduct](CODE_OF_CONDUCT.md) to define expected conduct in our community. Instances of abusive, harassing, or otherwise unacceptable behavior may be reported by contacting a project [maintainer](.github/CODEOWNERS)

## Contributing

Please refer to our [contributing](CONTRIBUTING.md) guide when you want to contribute to this project.

## Examples

In order to run the examples, please check the document [EK-TM4C123GXL](doc/EK-TM4C123GXL.md) first.

## License

hal-ti is licensed under the [MIT](https://choosealicense.com/licenses/mit/) [license](LICENSE) except the files and/or directories named in the [notice](NOTICE) file.

[embedded-infra-lib]: https://github.com/embedded-pro/embedded-infra-lib.git
