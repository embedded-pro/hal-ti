#ifndef VALIDATION_BOARD_TYPES_HPP
#define VALIDATION_BOARD_TYPES_HPP

#include "hal_tiva/tiva/Gpio.hpp"
#include "services/hil/HilPinId.hpp"
#include <cstdint>

namespace validation
{
    using services::HilPinAlias;
    using services::HilPinId;

    constexpr HilPinId Pin(hal::tiva::Port port, uint8_t index)
    {
        return HilPinId{ static_cast<uint8_t>(port), index };
    }

    constexpr hal::tiva::Port PortOf(HilPinId pin)
    {
        return static_cast<hal::tiva::Port>(pin.port);
    }

    struct UartPins
    {
        uint8_t index;
        HilPinId tx;
        HilPinId rx;
    };

    struct QeiPins
    {
        uint8_t index;
        HilPinId a;
        HilPinId b;
        HilPinId idx;
    };

    struct CanPins
    {
        uint8_t index;
        HilPinId rx;
        HilPinId tx;
    };
}

#endif
