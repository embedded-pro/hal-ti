#ifndef VALIDATION_BOARD_TYPES_HPP
#define VALIDATION_BOARD_TYPES_HPP

#include "hal_tiva/tiva/Gpio.hpp"
#include <cstdint>

namespace validation
{
    struct PinId
    {
        hal::tiva::Port port;
        uint8_t index;

        constexpr bool operator==(const PinId& other) const = default;
    };

    struct PinAlias
    {
        const char* name;
        PinId pin;
        hal::tiva::Drive pull = hal::tiva::Drive::None;
    };

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
