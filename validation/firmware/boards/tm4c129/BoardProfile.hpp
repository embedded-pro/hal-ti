#ifndef VALIDATION_BOARD_PROFILE_HPP
#define VALIDATION_BOARD_PROFILE_HPP

#include "hal_tiva/tiva/ClockTm4c129.hpp"
#include "hal_tiva/tiva/PinoutTableDefaultTm4c129.hpp"
#include "validation/firmware/BoardTypes.hpp"
#include <array>
#include <optional>

namespace validation::board
{
    using hal::tiva::Port;

    inline constexpr const char* name = "EK-TM4C1294XL";
    inline constexpr const char* family = "tm4c129";

    inline constexpr const char* portLetters = "ABCDEFGHJKLMNPQ";
    inline constexpr uint8_t maximumPinIndex = 7;
    inline constexpr uint8_t uarts = 8;
    inline constexpr uint8_t ssis = 4;
    inline constexpr uint8_t adcs = 2;
    inline constexpr uint8_t pwmModules = 1;
    inline constexpr uint8_t qeis = 1;
    inline constexpr uint8_t cans = 2;
    inline constexpr uint8_t comparators = 3;
    inline constexpr uint8_t watchDogs = 2;

    inline constexpr UartPins terminal{ 2, Pin(Port::D, 5), Pin(Port::D, 4) };
    inline constexpr HilPinId debugLed = Pin(Port::N, 1);
    inline constexpr std::array<HilPinId, 3> reservedPins{ { terminal.tx, terminal.rx, debugLed } };
    inline constexpr std::optional<UartPins> defaultUart = std::nullopt;
    inline constexpr std::optional<QeiPins> defaultQei = QeiPins{ 0, Pin(Port::L, 1), Pin(Port::L, 2), Pin(Port::L, 3) };
    inline constexpr std::optional<CanPins> defaultCan = CanPins{ 0, Pin(Port::A, 0), Pin(Port::A, 1) };

    inline constexpr std::array<HilPinAlias, 28> aliases{ {
        { "terminaltx", terminal.tx },
        { "terminalrx", terminal.rx },
        { "ain0", Pin(Port::E, 3) },
        { "ain1", Pin(Port::E, 2) },
        { "ain2", Pin(Port::E, 1) },
        { "ain10", Pin(Port::B, 4) },
        { "ain11", Pin(Port::B, 5) },
        { "m0pwm2", Pin(Port::F, 2) },
        { "m0pwm3", Pin(Port::F, 3) },
        { "m0pwm4", Pin(Port::G, 0) },
        { "m0pwm5", Pin(Port::G, 1) },
        { "m0pwm6", Pin(Port::K, 4) },
        { "m0pwm7", Pin(Port::K, 5) },
        { "qei0a", defaultQei->a },
        { "qei0b", defaultQei->b },
        { "qei0idx", defaultQei->idx },
        { "can0rx", defaultCan->rx },
        { "can0tx", defaultCan->tx },
        { "led0", Pin(Port::N, 3) },
        { "led1", Pin(Port::N, 2) },
        { "led2", Pin(Port::P, 2) },
        { "gpio0", Pin(Port::N, 4) },
        { "gpio1", Pin(Port::E, 4) },
        { "gpio2", Pin(Port::E, 5) },
        { "gpio3", Pin(Port::K, 0) },
        { "gpio4", Pin(Port::K, 1) },
        { "gpio5", Pin(Port::K, 2) },
        { "gpio6", Pin(Port::C, 6) },
    } };

    inline constexpr bool hasEthernet = true;

    inline void InitializeClocks()
    {
        hal::tiva::ConfigureClock(120000000, hal::tiva::crystalFrequency::_25_MHz, hal::tiva::oscillatorSource::main, hal::tiva::systemClockVco::_240_MHz, true);
    }
}

#endif
