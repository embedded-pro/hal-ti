#ifndef VALIDATION_BOARD_PROFILE_HPP
#define VALIDATION_BOARD_PROFILE_HPP

#include "hal_tiva/tiva/Adc.hpp"
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

    inline constexpr uint16_t availablePorts = 0x7fff;
    inline constexpr uint8_t uarts = 8;
    inline constexpr uint8_t ssis = 4;
    inline constexpr uint8_t adcs = 2;
    inline constexpr uint8_t pwmModules = 1;
    inline constexpr uint8_t qeis = 1;
    inline constexpr uint8_t cans = 2;
    inline constexpr uint8_t comparators = 3;
    inline constexpr uint8_t watchDogs = 2;

    inline constexpr PinId phaseA{ Port::E, 3 };
    inline constexpr PinId phaseB{ Port::E, 2 };
    inline constexpr PinId phaseC{ Port::E, 1 };
    inline constexpr PinId powerSupplyVoltage{ Port::B, 5 };
    inline constexpr PinId currentTotal{ Port::B, 4 };
    inline constexpr PinId encoderA{ Port::L, 1 };
    inline constexpr PinId encoderB{ Port::L, 2 };
    inline constexpr PinId encoderZ{ Port::L, 3 };
    inline constexpr PinId pwm1a{ Port::F, 2 };
    inline constexpr PinId pwm1b{ Port::F, 3 };
    inline constexpr PinId pwm2a{ Port::G, 0 };
    inline constexpr PinId pwm2b{ Port::G, 1 };
    inline constexpr PinId pwm3a{ Port::K, 4 };
    inline constexpr PinId pwm3b{ Port::K, 5 };
    inline constexpr PinId canRx{ Port::A, 0 };
    inline constexpr PinId canTx{ Port::A, 1 };

    inline constexpr UartPins terminal{ 2, { Port::D, 5 }, { Port::D, 4 } };
    inline constexpr std::optional<UartPins> defaultUart = std::nullopt;

    inline constexpr std::array<PinAlias, 29> aliases{ {
        { "terminaltx", terminal.tx },
        { "terminalrx", terminal.rx },
        { "phasea", phaseA },
        { "phaseb", phaseB },
        { "phasec", phaseC },
        { "vbus", powerSupplyVoltage },
        { "itotal", currentTotal },
        { "halla", { Port::E, 4 } },
        { "hallb", { Port::E, 5 } },
        { "hallc", { Port::E, 6 } },
        { "enca", encoderA },
        { "encb", encoderB },
        { "encz", encoderZ },
        { "pwm1a", pwm1a },
        { "pwm1b", pwm1b },
        { "pwm2a", pwm2a },
        { "pwm2b", pwm2b },
        { "pwm3a", pwm3a },
        { "pwm3b", pwm3b },
        { "canrx", canRx },
        { "cantx", canTx },
        { "ledop", { Port::N, 3 } },
        { "ledwarn", { Port::N, 2 } },
        { "ledfail", { Port::P, 2 } },
        { "perf", { Port::N, 4 } },
        { "id0", { Port::K, 0 }, hal::tiva::Drive::Up },
        { "id1", { Port::K, 1 }, hal::tiva::Drive::Up },
        { "id2", { Port::K, 2 }, hal::tiva::Drive::Up },
        { "pwrstatus", { Port::C, 6 }, hal::tiva::Drive::Up },
    } };

    inline constexpr uint8_t pwmModule = 0;
    inline constexpr std::array<PwmPhase, 3> pwmPhases{ {
        { 1, pwm1a, pwm1b },
        { 2, pwm2a, pwm2b },
        { 3, pwm3a, pwm3b },
    } };
    inline constexpr PwmTrigger pwmTrigger = PwmTrigger::zero;
    inline constexpr bool pwmSynchronous = false;
    inline constexpr bool hasFaultComparators = true;

    inline constexpr std::array<PinId, 5> phaseCurrentPins{ { phaseA, phaseB, phaseC, currentTotal, powerSupplyVoltage } };
    inline constexpr std::array<PinId, 1> supplyPins{ { powerSupplyVoltage } };
    inline constexpr hal::tiva::Adc::Trigger adcTrigger = hal::tiva::Adc::Trigger::pwmGenerator1;

    inline constexpr uint8_t qeiIndex = 0;
    inline constexpr uint8_t canIndex = 0;

    inline constexpr bool hasEthernet = true;

    inline void InitializeClocks()
    {
        hal::tiva::ConfigureClock(120000000, hal::tiva::crystalFrequency::_25_MHz, hal::tiva::oscillatorSource::main, hal::tiva::systemClockVco::_240_MHz, true);
    }
}

#endif
