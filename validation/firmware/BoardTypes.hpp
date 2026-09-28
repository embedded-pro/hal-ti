#ifndef VALIDATION_BOARD_TYPES_HPP
#define VALIDATION_BOARD_TYPES_HPP

#include "hal_tiva/tiva/Gpio.hpp"
#include "services/hil/PinId.hpp"
#include <cstdint>

namespace validation
{
    using services::hil::PinAlias;
    using services::hil::PinId;

    constexpr PinId Pin(hal::tiva::Port port, uint8_t index)
    {
        return PinId{ static_cast<uint8_t>(port), index };
    }

    constexpr hal::tiva::Port PortOf(PinId pin)
    {
        return static_cast<hal::tiva::Port>(pin.port);
    }

    struct UartPins
    {
        uint8_t index;
        PinId tx;
        PinId rx;
    };

    struct PwmPhase
    {
        uint8_t generator;
        PinId a;
        PinId b;
    };

    enum class PwmTrigger : uint8_t
    {
        none,
        zero,
        load,
    };
}

#endif
