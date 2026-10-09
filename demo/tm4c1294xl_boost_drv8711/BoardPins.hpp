#ifndef DEMO_TM4C1294XL_BOOST_DRV8711_BOARD_PINS_HPP
#define DEMO_TM4C1294XL_BOOST_DRV8711_BOARD_PINS_HPP

#include "hal_tiva/tiva/Gpio.hpp"
#include <cstdint>

namespace demo::pins
{
    struct Pin
    {
        hal::tiva::Port port;
        uint8_t index;
    };

    inline constexpr uint8_t driverSsi = 2;

    inline constexpr Pin sclk{ hal::tiva::Port::D, 3 };
    inline constexpr Pin sdatI{ hal::tiva::Port::D, 1 };
    inline constexpr Pin sdatO{ hal::tiva::Port::D, 0 };
    inline constexpr Pin chipSelect{ hal::tiva::Port::P, 2 };
    inline constexpr Pin nSleep{ hal::tiva::Port::E, 5 };
    inline constexpr Pin reset{ hal::tiva::Port::C, 7 };
    inline constexpr Pin stepAin1{ hal::tiva::Port::B, 2 };
    inline constexpr Pin dirAin2{ hal::tiva::Port::B, 3 };
    inline constexpr Pin bin1{ hal::tiva::Port::N, 2 };
    inline constexpr Pin bin2{ hal::tiva::Port::N, 3 };
    inline constexpr Pin nFault{ hal::tiva::Port::H, 2 };
    inline constexpr Pin nStall{ hal::tiva::Port::M, 3 };
    inline constexpr Pin potentiometer{ hal::tiva::Port::E, 4 };
}

#endif
