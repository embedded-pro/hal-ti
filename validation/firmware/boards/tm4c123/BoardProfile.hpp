#ifndef VALIDATION_BOARD_PROFILE_HPP
#define VALIDATION_BOARD_PROFILE_HPP

#include "hal_tiva/tiva/Adc.hpp"
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

    inline constexpr uint16_t availablePorts = 0x003f;
    inline constexpr uint8_t uarts = 8;
    inline constexpr uint8_t ssis = 4;
    inline constexpr uint8_t adcs = 2;
    inline constexpr uint8_t pwmModules = 2;
    inline constexpr uint8_t qeis = 2;
    inline constexpr uint8_t cans = 2;
    inline constexpr uint8_t comparators = 2;
    inline constexpr uint8_t watchDogs = 2;

    inline constexpr PinId phaseA{ Port::E, 3 };
    inline constexpr PinId phaseB{ Port::E, 2 };
    inline constexpr PinId phaseC{ Port::E, 1 };
    inline constexpr PinId powerSupplyVoltage{ Port::E, 0 };
    inline constexpr PinId currentTotal{ Port::E, 0 };
    inline constexpr PinId encoderA{ Port::D, 6 };
    inline constexpr PinId encoderB{ Port::D, 7 };
    inline constexpr PinId encoderZ{ Port::D, 3 };
    inline constexpr PinId pwm1a{ Port::B, 6 };
    inline constexpr PinId pwm1b{ Port::B, 7 };
    inline constexpr PinId pwm2a{ Port::B, 4 };
    inline constexpr PinId pwm2b{ Port::B, 5 };
    inline constexpr PinId pwm3a{ Port::E, 4 };
    inline constexpr PinId pwm3b{ Port::E, 5 };
    inline constexpr PinId canRx{ Port::F, 0 };
    inline constexpr PinId canTx{ Port::F, 3 };

    inline constexpr UartPins terminal{ 0, { Port::A, 1 }, { Port::A, 0 } };
    inline constexpr std::optional<UartPins> defaultUart = UartPins{ 1, { Port::B, 1 }, { Port::B, 0 } };

    inline constexpr std::array<PinAlias, 25> aliases{ {
        { "terminaltx", terminal.tx },
        { "terminalrx", terminal.rx },
        { "phasea", phaseA },
        { "phaseb", phaseB },
        { "phasec", phaseC },
        { "vbus", powerSupplyVoltage },
        { "itotal", currentTotal },
        { "halla", { Port::A, 4 } },
        { "hallb", { Port::A, 5 } },
        { "hallc", { Port::A, 6 } },
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
        { "ledop", { Port::F, 1 } },
        { "ledwarn", { Port::F, 1 } },
        { "ledfail", { Port::F, 1 } },
        { "perf", { Port::A, 2 } },
    } };

    inline constexpr uint8_t pwmModule = 0;
    inline constexpr std::array<PwmPhase, 3> pwmPhases{ {
        { 0, pwm1a, pwm1b },
        { 1, pwm2a, pwm2b },
        { 2, pwm3a, pwm3b },
    } };
    inline constexpr PwmTrigger pwmTrigger = PwmTrigger::load;
    inline constexpr bool pwmSynchronous = true;
    inline constexpr bool hasFaultComparators = false;

    inline constexpr std::array<PinId, 5> phaseCurrentPins{ { phaseA, phaseB, phaseC, currentTotal, powerSupplyVoltage } };
    inline constexpr std::array<PinId, 1> supplyPins{ { powerSupplyVoltage } };
    inline constexpr hal::tiva::Adc::Trigger adcTrigger = hal::tiva::Adc::Trigger::pwmGenerator0;

    inline constexpr uint8_t qeiIndex = 0;
    inline constexpr uint8_t canIndex = 0;

    inline constexpr bool hasEthernet = false;

    inline void InitializeClocks()
    {
        hal::tiva::ConfigureClock(hal::tiva::crystalFrequency::_16_MHz, hal::tiva::oscillatorSource::main);
    }
}

#endif
