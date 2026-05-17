#pragma once

#include "hal/interfaces/Pwm.hpp"
#include "hal_tiva/cortex/InterruptCortex.hpp"
#include "hal_tiva/tiva/Gpio.hpp"
#include "infra/util/BoundedVector.hpp"
#include "infra/util/EnumCast.hpp"
#include "infra/util/Function.hpp"
#include <chrono>
#include <cstdint>
#include <optional>
#include DEVICE_HEADER

namespace hal::tiva
{
    class Pwm
        : public hal::SingleChannelPwm
        , public hal::TwoChannelsPwm
        , public hal::ThreeChannelsPwm
        , public hal::FourChannelsPwm
    {
    public:
        enum class GeneratorIndex : uint8_t
        {
            generator0 = 0,
            generator1 = 1,
            generator2 = 2,
            generator3 = 3,
        };

        enum class NormalInterruptSource : uint8_t
        {
            countZero = 0,
            countLoad,
            comparatorAUp,
            comparatorADown,
            comparatorBUp,
            comparatorBDown,
        };

        struct Config
        {
            struct Control
            {
                enum class Mode
                {
                    edgeAligned = 0,
                    centerAligned,
                };

                enum class UpdateMode
                {
                    locally = 2,
                    globally = 3,
                };

                Mode mode = Mode::edgeAligned;
                UpdateMode updateMode = UpdateMode::globally;
                bool debugMode = false;

                uint32_t Value() const;
            };

            struct DeadTime
            {
                uint16_t fallInClockCycles = 0xffff;
                uint16_t riseInClockCycles = 0xffff;
            };

            enum class ClockDivisor
            {
                divisor1,
                divisor2,
                divisor4,
                divisor8,
                divisor16,
                divisor32,
                divisor64,
            };

            struct InterruptConfig
            {
                struct GeneratorInterrupt
                {
                    GeneratorIndex generator;
                    NormalInterruptSource source;
                };

                struct FaultConfig
                {
                    GeneratorIndex generator;
                    uint8_t enabledFaultInputs;
                    uint8_t enabledComparatorInputs;
                    bool latch;
                    uint16_t minimumFaultPeriod;
                };

                infra::BoundedVector<GeneratorInterrupt>::WithMaxSize<4> normalSources;
                infra::BoundedVector<FaultConfig>::WithMaxSize<4> faultConfigs;
                hal::InterruptPriority normalPriority;
                hal::InterruptPriority faultPriority;
            };

            bool channelAInverted = false;
            bool channelBInverted = false;
            Control control;
            ClockDivisor clockDivisor = ClockDivisor::divisor64;
            std::optional<DeadTime> deadTime;
            std::optional<InterruptConfig> interruptConfig;
        };

        struct PinChannel
        {
            enum class Trigger
            {
                countZero,
                countLoad,
                countComparatorAUp,
                countComparatorADown,
                countComparatorBUp,
                countComparatorBDown,
            };

            GeneratorIndex generator;

            GpioPin& pinA = dummyPin;
            GpioPin& pinB = dummyPin;

            bool usesChannelA = false;
            bool usesChannelB = false;

            std::optional<Trigger> trigger;
        };

        enum class FaultStatus : uint8_t
        {
            generator0 = 1 << 0,
            generator1 = 1 << 1,
            generator2 = 1 << 2,
            generator3 = 1 << 3,
        };

        enum class FaultInput : uint8_t
        {
            fault0 = 1 << 0,
            fault1 = 1 << 1,
            fault2 = 1 << 2,
            fault3 = 1 << 3,
        };

        enum class FaultInputComparator : uint8_t
        {
            comparator0 = 1 << 0,
            comparator1 = 1 << 1,
            comparator2 = 1 << 2,
            comparator3 = 1 << 3,
            comparator4 = 1 << 4,
            comparator5 = 1 << 5,
            comparator6 = 1 << 6,
            comparator7 = 1 << 7,
        };

        struct NormalEvent
        {
            GeneratorIndex generator;
            NormalInterruptSource source;
        };

        struct FaultEvent
        {
            FaultStatus generatorStatus;
            std::array<FaultInput, 4> inputsByGenerator;
            std::array<FaultInputComparator, 4> comparatorInputsByGenerator;
        };

        Pwm(uint8_t pwmIndex,
            infra::MemoryRange<PinChannel> channels,
            const Config& config,
            const infra::Function<void(NormalEvent)>& onNormalInterrupt,
            const infra::Function<void(FaultEvent)>& onFault);
        ~Pwm();

        void SetBaseFrequency(hal::Hertz baseFrequency) override;
        void Start(hal::Percent globalDutyCycle) override;
        void Start(hal::Percent dutyCycle1, hal::Percent dutyCycle2) override;
        void Start(hal::Percent dutyCycle1, hal::Percent dutyCycle2, hal::Percent dutyCycle3) override;
        void Start(hal::Percent dutyCycle1, hal::Percent dutyCycle2, hal::Percent dutyCycle3, hal::Percent dutyCycle4) override;
        void Stop() override;

        static uint16_t CalculateDeadTimeCycles(std::chrono::nanoseconds deadTime, Config::ClockDivisor divisor);

    private:
        struct PwmChannelType
        {
            __IO uint32_t CTL;
            __IO uint32_t INTEN;
            __IO uint32_t RIS;
            __IO uint32_t ISC;
            __IO uint32_t LOAD;
            __IO uint32_t COUNT;
            __IO uint32_t CMPA;
            __IO uint32_t CMPB;
            __IO uint32_t GENA;
            __IO uint32_t GENB;
            __IO uint32_t DBCTL;
            __IO uint32_t DBRISE;
            __IO uint32_t DBFALL;
            __IO uint32_t FLTSRC0;
            __IO uint32_t FLTSRC1;
            __IO uint32_t MINFLTPER;
        };

        static constexpr std::array<uint32_t, 4> peripheralPwmChannelOffsetArray = { {
            0x00000040,
            0x00000080,
            0x000000C0,
            0x00000100,
        } };

        static volatile PwmChannelType* const PwmChannel(uint32_t pwmBaseAddress, GeneratorIndex generatorIndex)
        {
            return reinterpret_cast<volatile PwmChannelType* const>(pwmBaseAddress + peripheralPwmChannelOffsetArray[infra::enum_cast(generatorIndex)]);
        }

        struct Generator
        {
            Generator(PinChannel& pins, uint32_t pwmOffset, GeneratorIndex index, std::optional<PinChannel::Trigger> trigger);

            std::optional<PeripheralPin> a;
            std::optional<PeripheralPin> b;
            volatile PwmChannelType* const address;
            uint32_t enable = 0;
            uint32_t generatorId = 0;
            std::optional<PinChannel::Trigger> trigger;
        };

        struct GeneratorInterruptSlot
        {
            GeneratorInterruptSlot(Pwm& owner, IRQn_Type irq, GeneratorIndex gen);

            Pwm& owner;
            GeneratorIndex gen;
            hal::ImmediateInterruptHandler handler;
        };

        struct FaultInterruptSlot
        {
            FaultInterruptSlot(Pwm& owner, IRQn_Type irq);

            Pwm& owner;
            hal::ImmediateInterruptHandler handler;
        };

        uint8_t pwmIndex;
        const Config& config;
        infra::BoundedVector<Generator>::WithMaxSize<4> generators;
        infra::Function<void(NormalEvent)> onNormalInterrupt;
        infra::Function<void(FaultEvent)> onFault;
        std::optional<GeneratorInterruptSlot> generatorHandlers[4];
        std::optional<FaultInterruptSlot> faultHandler;

        void Initialize();
        void ConfigureNormalInterrupts(const Config::InterruptConfig& interruptConfig);
        void ConfigureFaultInterrupts(const Config::InterruptConfig& interruptConfig);
        void GeneratorConfiguration(Generator& generator) const;
        void EnableDeadBand(Generator& generator) const;
        void EnableGenerator(Generator& generator) const;
        void DisableGenerator(Generator& generator) const;
        void EnableOutput(const Generator& generator) const;
        void DisableOutput(const Generator& generator) const;
        void SetComparator(Generator& generator, const hal::Percent& dutyCycle) const;
        void Sync() const;
        uint32_t GetLoad(const Generator& generator) const;
        void EnableClock() const;
        void DisableClock() const;
        void HandleGeneratorIrq(GeneratorIndex gen);
        void HandleFaultIrq();
    };
}
