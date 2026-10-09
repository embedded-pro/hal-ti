#ifndef DEMO_EK_TM4C123XGL_BOOSTXL_K350QVG_S1_BOARD_PINS_HPP
#define DEMO_EK_TM4C123XGL_BOOSTXL_K350QVG_S1_BOARD_PINS_HPP

#include "hal_tiva/tiva/Gpio.hpp"
#include <cstdint>

namespace demo::pins
{
    struct Pin
    {
        hal::tiva::Port port;
        uint8_t index;
    };

    inline constexpr uint8_t lcdSsi = 2;

    inline constexpr Pin lcdClock{ hal::tiva::Port::B, 4 };
    inline constexpr Pin lcdMosi{ hal::tiva::Port::B, 7 };
    inline constexpr Pin lcdChipSelect{ hal::tiva::Port::A, 4 };
    inline constexpr Pin lcdDataCommand{ hal::tiva::Port::A, 5 };
    inline constexpr Pin lcdReset{ hal::tiva::Port::D, 7 };
}

#endif
