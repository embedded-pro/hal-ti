#ifndef HAL_PWM_TIVA_HPP
#define HAL_PWM_TIVA_HPP

#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "hal_tiva/tiva/Gpio.hpp"
#include "infra/util/BoundedVector.hpp"
#include "infra/util/EnumCast.hpp"
#include <optional>

namespace hal::tiva
{
    class SynchronousPwm
        : public hal::SynchronousSingleChannelPwm
        , public hal::SynchronousTwoChannelsPwm
        , public hal::SynchronousThreeChannelsPwm
        , public hal::SynchronousFourChannelsPwm
    {
    public:
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
                uint8_t fallInClockCycles = 0xff;
                uint8_t riseInClockCycles = 0xff;
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

            enum class Trigger
            {
                countZero,
                countLoad,
                countComparatorAUp,
                countComparatorADown,
                countComparatorBUp,
                countComparatorBDown,
            };

            bool channelAInverted = false;
            bool channelBInverted = false;
            Control control;
            ClockDivisor clockDivisor = ClockDivisor::divisor64;
            std::optional<DeadTime> deadTime;
            std::optional<Trigger> trigger;
        };

        enum class GeneratorIndex : uint8_t
        {
            generator0 = 0,
            generator1 = 1,
            generator2 = 2,
            generator3 = 3,
        };

        struct PinChannel
        {
            GeneratorIndex generator;

            GpioPin& pinA = dummyPin;
            GpioPin& pinB = dummyPin;

            bool usesChannelA = false;
            bool usesChannelB = false;
        };

        SynchronousPwm(uint8_t aPwmIndex, infra::MemoryRange<PinChannel> channels, const Config& config);
        ~SynchronousPwm();

        void SetBaseFrequency(hal::Hertz baseFrequency) override;
        void Start(hal::Percent globalDutyCycle) override;
        void Start(hal::Percent dutyCycle1, hal::Percent dutyCycle2) override;
        void Start(hal::Percent dutyCycle1, hal::Percent dutyCycle2, hal::Percent dutyCycle3) override;
        void Start(hal::Percent dutyCycle1, hal::Percent dutyCycle2, hal::Percent dutyCycle3, hal::Percent dutyCycle4) override;
        void Stop() override;

    private:
        struct PwmChannelType
        {
            __IO uint32_t CTL;       /*!< PWM Control                            */
            __IO uint32_t INTEN;     /*!< PWM Interrupt and Trigger Enable       */
            __IO uint32_t RIS;       /*!< PWM Raw Interrupt Status               */
            __IO uint32_t ISC;       /*!< PWM Interrupt Status and Clear         */
            __IO uint32_t LOAD;      /*!< PWM Load                               */
            __IO uint32_t COUNT;     /*!< PWM Counter                            */
            __IO uint32_t CMPA;      /*!< PWM Compare A                          */
            __IO uint32_t CMPB;      /*!< PWM Compare B                          */
            __IO uint32_t GENA;      /*!< PWM Generator A Control                */
            __IO uint32_t GENB;      /*!< PWM Generator B Control                */
            __IO uint32_t DBCTL;     /*!< PWM Dead-Band Control                  */
            __IO uint32_t DBRISE;    /*!< PWM Dead-Band Rising-Edge Delay        */
            __IO uint32_t DBFALL;    /*!< PWM Dead-Band Falling-Edge-Delay       */
            __IO uint32_t FLTSRC0;   /*!< PWM Fault Source 0                     */
            __IO uint32_t FLTSRC1;   /*!< PWM Fault Source 1                     */
            __IO uint32_t MINFLTPER; /*!< PWM Minimum Fault Period               */
        };

        static constexpr std::array<uint32_t, 4> peripheralPwmChannelOffsetArray = { {
            0x00000040, /* Channel 0 */
            0x00000080, /* Channel 1 */
            0x000000C0, /* Channel 2 */
            0x00000100, /* Channel 3 */
        } };

        static PwmChannelType* const PwmChannel(uint32_t pwmBaseAddress, GeneratorIndex generatorIndex)
        {
            return reinterpret_cast<PwmChannelType* const>(pwmBaseAddress + peripheralPwmChannelOffsetArray[infra::enum_cast(generatorIndex)]);
        }

        struct Generator
        {
            Generator(PinChannel& pins, uint32_t pwmOffset, GeneratorIndex index);

            infra::Optional<PeripheralPin> a;
            infra::Optional<PeripheralPin> b;
            PwmChannelType* const address;
            uint32_t enable = 0;
        };

        uint8_t pwmIndex;
        const Config& config;
        infra::BoundedVector<Generator>::WithMaxSize<4> generators;
        infra::MemoryRange<PWM0_Type* const> pwmArray;

    private:
        void Initialize();
        void GeneratorConfiguration(Generator& generator);
        void SetComparator(Generator& generator, hal::Percent& dutyCycle);
        void Sync();
        uint32_t GetLoad(Generator& generator);
        void EnableClock();
        void DisableClock();
    };
}

#endif
