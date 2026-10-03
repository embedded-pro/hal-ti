#ifndef VALIDATION_BOARD_PROFILE_HPP
#define VALIDATION_BOARD_PROFILE_HPP

#include "hal_tiva/tiva/ClockTm4c123.hpp"
#include "hal_tiva/tiva/PinoutTableDefaultTm4c123.hpp"
#include "validation/firmware/BoardTypes.hpp"
#include <array>
#include <optional>

namespace validation::board
{
    using hal::tiva::Port;

    inline constexpr const char* name = "EK-TM4C123GXL";
    inline constexpr const char* family = "tm4c123";

    inline constexpr const char* portLetters = "ABCDEF";
    inline constexpr uint8_t maximumPinIndex = 7;
    inline constexpr uint8_t uarts = 8;
    inline constexpr uint8_t ssis = 4;
    inline constexpr uint8_t adcs = 2;
    inline constexpr uint8_t pwmModules = 2;
    inline constexpr uint8_t qeis = 2;
    inline constexpr uint8_t cans = 2;
    inline constexpr uint8_t comparators = 2;
    inline constexpr uint8_t watchDogs = 2;

    inline constexpr UartPins terminal{ 0, Pin(Port::A, 1), Pin(Port::A, 0) };
    // Blue LED: the green one (PF3) is the CAN0 transmit pin.
    inline constexpr HilPinId debugLed = Pin(Port::F, 2);
    inline constexpr std::array<HilPinId, 3> reservedPins{ { terminal.tx, terminal.rx, debugLed } };
    inline constexpr std::optional<UartPins> defaultUart = UartPins{ 1, Pin(Port::B, 1), Pin(Port::B, 0) };
    inline constexpr std::optional<QeiPins> defaultQei = QeiPins{ 0, Pin(Port::D, 6), Pin(Port::D, 7), Pin(Port::D, 3) };
    inline constexpr std::optional<CanPins> defaultCan = CanPins{ 0, Pin(Port::F, 0), Pin(Port::F, 3) };

    inline constexpr std::array<HilPinAlias, 22> aliases{ {
        { "terminaltx", terminal.tx },
        { "terminalrx", terminal.rx },
        { "ain0", Pin(Port::E, 3) },
        { "ain1", Pin(Port::E, 2) },
        { "ain2", Pin(Port::E, 1) },
        { "ain3", Pin(Port::E, 0) },
        { "m0pwm0", Pin(Port::B, 6) },
        { "m0pwm1", Pin(Port::B, 7) },
        { "m0pwm2", Pin(Port::B, 4) },
        { "m0pwm3", Pin(Port::B, 5) },
        { "m0pwm4", Pin(Port::E, 4) },
        { "m0pwm5", Pin(Port::E, 5) },
        { "qei0a", defaultQei->a },
        { "qei0b", defaultQei->b },
        { "qei0idx", defaultQei->idx },
        { "can0rx", defaultCan->rx },
        { "can0tx", defaultCan->tx },
        { "led0", Pin(Port::F, 1) },
        { "gpio0", Pin(Port::A, 2) },
        { "gpio1", Pin(Port::A, 4) },
        { "gpio2", Pin(Port::A, 5) },
        { "gpio3", Pin(Port::A, 6) },
    } };

    inline constexpr bool hasEthernet = false;

    inline void InitializeClocks()
    {
        hal::tiva::ConfigureClock(hal::tiva::crystalFrequency::_16_MHz, hal::tiva::oscillatorSource::main);
    }
}

#endif
