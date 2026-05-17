#include "hal_tiva/tiva/Pwm.hpp"
#include "hal_tiva/tiva/Gpio.hpp"
#include "infra/util/EnumCast.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace
{
    extern "C" void Pwm0Generator0_Handler()
    {
        hal::InterruptTable::Instance().Invoke(PWM0_0_IRQn);
    }

    extern "C" void Pwm0Generator1_Handler()
    {
        hal::InterruptTable::Instance().Invoke(PWM0_1_IRQn);
    }

    extern "C" void Pwm0Generator2_Handler()
    {
        hal::InterruptTable::Instance().Invoke(PWM0_2_IRQn);
    }

    extern "C" void Pwm0Generator3_Handler()
    {
        hal::InterruptTable::Instance().Invoke(PWM0_3_IRQn);
    }

    extern "C" void Pwm0Fault_Handler()
    {
        hal::InterruptTable::Instance().Invoke(PWM0_FAULT_IRQn);
    }

#if defined(TM4C123)
    extern "C" void Pwm1Generator0_Handler()
    {
        hal::InterruptTable::Instance().Invoke(PWM1_0_IRQn);
    }

    extern "C" void Pwm1Generator1_Handler()
    {
        hal::InterruptTable::Instance().Invoke(PWM1_1_IRQn);
    }

    extern "C" void Pwm1Generator2_Handler()
    {
        hal::InterruptTable::Instance().Invoke(PWM1_2_IRQn);
    }

    extern "C" void Pwm1Generator3_Handler()
    {
        hal::InterruptTable::Instance().Invoke(PWM1_3_IRQn);
    }

    extern "C" void Pwm1Fault_Handler()
    {
        hal::InterruptTable::Instance().Invoke(PWM1_FAULT_IRQn);
    }
#endif

    extern "C" uint32_t SystemCoreClock;

#if defined(TM4C129)
    constexpr std::size_t numberOfPwms = 1;
#else
    constexpr std::size_t numberOfPwms = 2;
#endif

    constexpr std::array<uint32_t, numberOfPwms> peripheralPwmArray = { {
        PWM0_BASE,
#if defined(TM4C123)
        PWM1_BASE,
#endif
    } };

    const infra::MemoryRange<PWM0_Type* const> peripheralPwm =
        infra::ReinterpretCastMemoryRange<PWM0_Type* const>(infra::MakeRange(peripheralPwmArray));

    struct PwmIrqInfo
    {
        std::array<IRQn_Type, 4> generatorIrqs;
        IRQn_Type faultIrq;
    };

    constexpr std::array<PwmIrqInfo, numberOfPwms> peripheralPwmIrqs = { {
        { { PWM0_0_IRQn, PWM0_1_IRQn, PWM0_2_IRQn, PWM0_3_IRQn }, PWM0_FAULT_IRQn },
#if defined(TM4C123)
        { { PWM1_0_IRQn, PWM1_1_IRQn, PWM1_2_IRQn, PWM1_3_IRQn }, PWM1_FAULT_IRQn },
#endif
    } };

    constexpr const uint32_t PWM_INTEN_INTFAULT3 = 0x00080000;
    constexpr const uint32_t PWM_INTEN_INTFAULT2 = 0x00040000;
    constexpr const uint32_t PWM_INTEN_INTFAULT1 = 0x00020000;
    constexpr const uint32_t PWM_INTEN_INTFAULT0 = 0x00010000;

    constexpr const uint32_t PWM_ISC_INTFAULT3 = 0x00080000;
    constexpr const uint32_t PWM_ISC_INTFAULT2 = 0x00040000;
    constexpr const uint32_t PWM_ISC_INTFAULT1 = 0x00020000;
    constexpr const uint32_t PWM_ISC_INTFAULT0 = 0x00010000;

    constexpr const uint32_t PWM_CHANNEL_CTL_LATCH    = 0x00040000;
    constexpr const uint32_t PWM_CHANNEL_CTL_MINFLTPER = 0x00020000;
    constexpr const uint32_t PWM_CHANNEL_CTL_FLTSRC   = 0x00010000;
    constexpr const uint32_t PWM_CHANNEL_CTL_ENABLE   = 0x00000001;

    constexpr const uint32_t PWM_CHANNEL_INTEN_TRCMPBD  = 0x00002000;
    constexpr const uint32_t PWM_CHANNEL_INTEN_TRCMPBU  = 0x00001000;
    constexpr const uint32_t PWM_CHANNEL_INTEN_TRCMPAD  = 0x00000800;
    constexpr const uint32_t PWM_CHANNEL_INTEN_TRCMPAU  = 0x00000400;
    constexpr const uint32_t PWM_CHANNEL_INTEN_TRCNTLOAD = 0x00000200;
    constexpr const uint32_t PWM_CHANNEL_INTEN_TRCNTZERO = 0x00000100;

    constexpr const uint32_t PWM_CHANNEL_INTEN_INTCMPBD  = 0x00000020;
    constexpr const uint32_t PWM_CHANNEL_INTEN_INTCMPBU  = 0x00000010;
    constexpr const uint32_t PWM_CHANNEL_INTEN_INTCMPAD  = 0x00000008;
    constexpr const uint32_t PWM_CHANNEL_INTEN_INTCMPAU  = 0x00000004;
    constexpr const uint32_t PWM_CHANNEL_INTEN_INTCNTLOAD = 0x00000002;
    constexpr const uint32_t PWM_CHANNEL_INTEN_INTCNTZERO = 0x00000001;

    constexpr const uint32_t PWM_CHANNEL_ISC_NORMAL_MASK = 0x0000003F;

    constexpr const uint32_t PWM_CHANNEL_GENA_ACTCMPAU_ONE  = 0x00000030;
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTCMPAD_ZERO = 0x00000080;
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTLOAD_ONE   = 0x0000000C;

    constexpr const uint32_t PWM_CHANNEL_GENB_ACTCMPBU_ONE  = 0x00000300;
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTCMPBD_ZERO = 0x00000800;
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTLOAD_ONE   = 0x0000000C;

    constexpr const uint32_t PWM_CHANNEL_DBCTL_ENABLE = 0x00000001;

    constexpr const uint32_t SYSCTL_RCC_USEPWMDIV = 0x00100000;
    constexpr const uint32_t SYSCTL_RCC_PWMDIV_M  = 0x000E0000;
    constexpr const uint32_t SYSCTL_RCC_PWMDIV_2  = 0x00000000;
    constexpr const uint32_t SYSCTL_RCC_PWMDIV_4  = 0x00020000;
    constexpr const uint32_t SYSCTL_RCC_PWMDIV_8  = 0x00040000;
    constexpr const uint32_t SYSCTL_RCC_PWMDIV_16 = 0x00060000;
    constexpr const uint32_t SYSCTL_RCC_PWMDIV_32 = 0x00080000;
    constexpr const uint32_t SYSCTL_RCC_PWMDIV_64 = 0x000A0000;

    constexpr const uint32_t PWM_CC_USEPWMDIV = 0x00000100;
    constexpr const uint32_t PWM_CC_PWMDIV_M  = 0x00000007;
    constexpr const uint32_t PWM_CC_PWMDIV_2  = 0x00000000;
    constexpr const uint32_t PWM_CC_PWMDIV_4  = 0x00000001;
    constexpr const uint32_t PWM_CC_PWMDIV_8  = 0x00000002;
    constexpr const uint32_t PWM_CC_PWMDIV_16 = 0x00000003;
    constexpr const uint32_t PWM_CC_PWMDIV_32 = 0x00000004;
    constexpr const uint32_t PWM_CC_PWMDIV_64 = 0x00000005;

    constexpr const uint32_t SYSCTL_DC1_PWM1 = 0x00200000;
    constexpr const uint32_t SYSCTL_DC1_PWM0 = 0x00100000;

    constexpr const std::array<uint32_t, 6> normalInterruptBit = { {
        PWM_CHANNEL_INTEN_INTCNTZERO,
        PWM_CHANNEL_INTEN_INTCNTLOAD,
        PWM_CHANNEL_INTEN_INTCMPAU,
        PWM_CHANNEL_INTEN_INTCMPAD,
        PWM_CHANNEL_INTEN_INTCMPBU,
        PWM_CHANNEL_INTEN_INTCMPBD,
    } };

    constexpr const std::array<uint32_t, 6> triggerType = { {
        PWM_CHANNEL_INTEN_TRCNTZERO,
        PWM_CHANNEL_INTEN_TRCNTLOAD,
        PWM_CHANNEL_INTEN_TRCMPAU,
        PWM_CHANNEL_INTEN_TRCMPAD,
        PWM_CHANNEL_INTEN_TRCMPBU,
        PWM_CHANNEL_INTEN_TRCMPBD,
    } };

    constexpr const std::array<std::pair<hal::tiva::PinConfigPeripheral, hal::tiva::PinConfigPeripheral>, 4> pinConfigPeripheral = { {
        { hal::tiva::PinConfigPeripheral::pwmChannel0, hal::tiva::PinConfigPeripheral::pwmChannel1 },
        { hal::tiva::PinConfigPeripheral::pwmChannel2, hal::tiva::PinConfigPeripheral::pwmChannel3 },
        { hal::tiva::PinConfigPeripheral::pwmChannel4, hal::tiva::PinConfigPeripheral::pwmChannel5 },
        { hal::tiva::PinConfigPeripheral::pwmChannel6, hal::tiva::PinConfigPeripheral::pwmChannel7 },
    } };

    constexpr const std::array<uint32_t, 4> faultIntEnBit = { {
        PWM_INTEN_INTFAULT0,
        PWM_INTEN_INTFAULT1,
        PWM_INTEN_INTFAULT2,
        PWM_INTEN_INTFAULT3,
    } };

    constexpr const std::array<uint32_t, 7> clockDivisor = { {
#if defined(TM4C123)
        0,
        SYSCTL_RCC_PWMDIV_2  | SYSCTL_RCC_USEPWMDIV,
        SYSCTL_RCC_PWMDIV_4  | SYSCTL_RCC_USEPWMDIV,
        SYSCTL_RCC_PWMDIV_8  | SYSCTL_RCC_USEPWMDIV,
        SYSCTL_RCC_PWMDIV_16 | SYSCTL_RCC_USEPWMDIV,
        SYSCTL_RCC_PWMDIV_32 | SYSCTL_RCC_USEPWMDIV,
        SYSCTL_RCC_PWMDIV_64 | SYSCTL_RCC_USEPWMDIV,
#else
        0,
        PWM_CC_PWMDIV_2  | PWM_CC_USEPWMDIV,
        PWM_CC_PWMDIV_4  | PWM_CC_USEPWMDIV,
        PWM_CC_PWMDIV_8  | PWM_CC_USEPWMDIV,
        PWM_CC_PWMDIV_16 | PWM_CC_USEPWMDIV,
        PWM_CC_PWMDIV_32 | PWM_CC_USEPWMDIV,
        PWM_CC_PWMDIV_64 | PWM_CC_USEPWMDIV,
#endif
    } };

    void SetClockDivisor(PWM0_Type* const pwmBase, hal::tiva::Pwm::Config::ClockDivisor divisor)
    {
#if defined(TM4C123)
        really_assert(SYSCTL->DC1 & (SYSCTL_DC1_PWM0 | SYSCTL_DC1_PWM1));
        SYSCTL->RCC = ((SYSCTL->RCC & ~(SYSCTL_RCC_USEPWMDIV | SYSCTL_RCC_PWMDIV_M)) | clockDivisor[static_cast<std::size_t>(divisor)]);
#else
        pwmBase->CC = ((pwmBase->CC & ~(PWM_CC_USEPWMDIV | PWM_CC_PWMDIV_M)) | clockDivisor[static_cast<std::size_t>(divisor)]);
#endif
    }

    uint32_t GetClockDivisor(PWM0_Type* const pwmBase)
    {
#if defined(TM4C123)
        auto result = (SYSCTL->RCC & SYSCTL_RCC_PWMDIV_M) >> 17;
        if (!(SYSCTL->RCC & SYSCTL_RCC_USEPWMDIV))
            return 1;
        else
            return 1U << (result + 1);
#else
        auto result = pwmBase->CC & PWM_CC_PWMDIV_M;
        if (!(pwmBase->CC & PWM_CC_USEPWMDIV))
            return 1;
        else
            return 1U << (result + 1);
#endif
    }

    float GetSystemCoreClock()
    {
        return static_cast<float>(SystemCoreClock);
    }

    uint32_t ToPeriod(PWM0_Type* const pwmBase, const hal::Hertz& baseFrequency)
    {
        auto pwmClock = SystemCoreClock / GetClockDivisor(pwmBase);
        return pwmClock / baseFrequency.Value();
    }

    bool IsCenterAligned(hal::tiva::Pwm::Config::Control::Mode mode)
    {
        return mode == hal::tiva::Pwm::Config::Control::Mode::centerAligned;
    }
}

namespace hal::tiva
{
    uint32_t Pwm::Config::Control::Value() const
    {
        auto value = static_cast<uint32_t>(mode) << 1;

        value |= static_cast<uint32_t>(debugMode) << 2;

        value |= static_cast<uint32_t>(updateMode == UpdateMode::globally) << 3;
        value |= static_cast<uint32_t>(updateMode == UpdateMode::globally) << 4;
        value |= static_cast<uint32_t>(updateMode == UpdateMode::globally) << 5;

        value |= static_cast<uint32_t>(updateMode) << 6;
        value |= static_cast<uint32_t>(updateMode) << 8;

        return value & 0x7fffe;
    }

    Pwm::GeneratorInterruptSlot::GeneratorInterruptSlot(Pwm& owner, IRQn_Type irq, hal::InterruptPriority priority, GeneratorIndex gen)
        : owner(owner)
        , gen(gen)
        , handler(irq, priority, [this]()
              {
                  this->owner.HandleGeneratorIrq(this->gen);
              })
    {}

    Pwm::FaultInterruptSlot::FaultInterruptSlot(Pwm& owner, IRQn_Type irq, hal::InterruptPriority priority)
        : owner(owner)
        , handler(irq, priority, [this]()
              {
                  this->owner.HandleFaultIrq();
              })
    {}

    Pwm::Generator::Generator(PinChannel& pins, uint32_t pwmOffset, GeneratorIndex index, std::optional<PinChannel::Trigger> trigger)
        : address(PwmChannel(pwmOffset, index))
        , generatorId(1 << static_cast<uint32_t>(index))
        , trigger(trigger)
    {
        auto pinIndex = static_cast<uint8_t>(index) * 2;
        auto pinConfig = pinConfigPeripheral.at(infra::enum_cast(index));

        if (pins.usesChannelA)
        {
            a.emplace(pins.pinA, pinConfig.first);
            enable |= 1 << pinIndex;
        }

        if (pins.usesChannelB)
        {
            b.emplace(pins.pinB, pinConfig.second);
            enable |= 1 << (pinIndex + 1);
        }
    }

    Pwm::Pwm(uint8_t aPwmIndex, infra::MemoryRange<PinChannel> channels, const Config& aConfig,
        const infra::Function<void(NormalEvent)>& aNormalInterrupt,
        const infra::Function<void(FaultEvent)>& aFault)
        : pwmIndex(aPwmIndex)
        , config(aConfig)
        , onNormalInterrupt(aNormalInterrupt)
        , onFault(aFault)
    {
        really_assert(!channels.empty() && channels.size() <= generators.max_size());

        for (auto& channel : channels)
            generators.emplace_back(channel, peripheralPwmArray[pwmIndex], channel.generator, channel.trigger);

        Initialize();
    }

    Pwm::~Pwm()
    {
        Stop();

        for (auto& gen : generators)
            gen.address->INTEN &= ~PWM_CHANNEL_ISC_NORMAL_MASK;

        peripheralPwm[pwmIndex]->INTEN = 0;

        for (auto& h : generatorHandlers)
            h.reset();
        faultHandler.reset();

        DisableClock();
    }

    void Pwm::Initialize()
    {
        EnableClock();
        SetClockDivisor(peripheralPwm[pwmIndex], config.clockDivisor);

        for (auto& gen : generators)
            GeneratorConfiguration(gen);

        if (!config.interruptConfig.has_value())
            return;

        ConfigureNormalInterrupts(*config.interruptConfig);
        ConfigureFaultInterrupts(*config.interruptConfig);
    }

    void Pwm::ConfigureNormalInterrupts(const Config::InterruptConfig& interruptConfig)
    {
        for (const auto& normalSource : interruptConfig.normalSources)
        {
            const auto genIdx = static_cast<uint8_t>(normalSource.generator);
            really_assert(genIdx < 4);

            auto* channel = PwmChannel(peripheralPwmArray[pwmIndex], normalSource.generator);
            channel->INTEN |= normalInterruptBit[static_cast<uint8_t>(normalSource.source)];

            if (!generatorHandlers[genIdx].has_value())
                generatorHandlers[genIdx].emplace(*this, peripheralPwmIrqs[pwmIndex].generatorIrqs[genIdx], interruptConfig.normalPriority, normalSource.generator);
        }
    }

    void Pwm::ConfigureFaultInterrupts(const Config::InterruptConfig& interruptConfig)
    {
        bool hasFault = false;
        for (const auto& faultConfig : interruptConfig.faultConfigs)
        {
            const auto genIdx = static_cast<uint8_t>(faultConfig.generator);
            really_assert(genIdx < 4);

            auto* channel = PwmChannel(peripheralPwmArray[pwmIndex], faultConfig.generator);
            channel->FLTSRC0 = faultConfig.enabledFaultInputs & 0x0F;
            channel->FLTSRC1 = faultConfig.enabledComparatorInputs & 0xFF;

            if (faultConfig.minimumFaultPeriod > 0)
            {
                channel->MINFLTPER = faultConfig.minimumFaultPeriod;
                channel->CTL |= PWM_CHANNEL_CTL_MINFLTPER;
            }

            auto ctlFaultBits = PWM_CHANNEL_CTL_FLTSRC;
            if (faultConfig.latch)
                ctlFaultBits |= PWM_CHANNEL_CTL_LATCH;
            channel->CTL |= ctlFaultBits;

            peripheralPwm[pwmIndex]->INTEN |= faultIntEnBit[genIdx];
            hasFault = true;
        }

        if (hasFault)
            faultHandler.emplace(*this, peripheralPwmIrqs[pwmIndex].faultIrq, interruptConfig.faultPriority);
        
    }

    void Pwm::SetBaseFrequency(hal::Hertz baseFrequency)
    {
        auto load = ToPeriod(peripheralPwm[pwmIndex], baseFrequency);
        load = IsCenterAligned(config.control.mode) ? load / 2 : load - 1;
        really_assert(load > 0 && load <= 0xffff);

        for (auto& gen : generators)
            if (gen.a || gen.b)
                gen.address->LOAD = load;

        Sync();
    }

    void Pwm::Start(hal::Percent dutyCycle)
    {
        really_assert(generators.size() >= 1);

        for (auto& gen : generators)
            SetComparator(gen, dutyCycle);

        Sync();
    }

    void Pwm::Start(hal::Percent dutyCycle1, hal::Percent dutyCycle2)
    {
        really_assert(generators.size() == 2);

        SetComparator(generators[0], dutyCycle1);
        SetComparator(generators[1], dutyCycle2);

        Sync();
    }

    void Pwm::Start(hal::Percent dutyCycle1, hal::Percent dutyCycle2, hal::Percent dutyCycle3)
    {
        really_assert(generators.size() == 3);

        SetComparator(generators[0], dutyCycle1);
        SetComparator(generators[1], dutyCycle2);
        SetComparator(generators[2], dutyCycle3);

        Sync();
    }

    void Pwm::Start(hal::Percent dutyCycle1, hal::Percent dutyCycle2, hal::Percent dutyCycle3, hal::Percent dutyCycle4)
    {
        really_assert(generators.size() == 4);

        SetComparator(generators[0], dutyCycle1);
        SetComparator(generators[1], dutyCycle2);
        SetComparator(generators[2], dutyCycle3);
        SetComparator(generators[3], dutyCycle4);

        Sync();
    }

    void Pwm::Stop()
    {
        for (auto& gen : generators)
        {
            DisableGenerator(gen);
            DisableOutput(gen);
        }

        Sync();
    }

    void Pwm::GeneratorConfiguration(Generator& generator) const
    {
        if (generator.a || generator.b)
        {
            generator.address->CTL |= config.control.Value();
            generator.address->GENA = IsCenterAligned(config.control.mode)
                ? (PWM_CHANNEL_GENA_ACTCMPAU_ONE | PWM_CHANNEL_GENA_ACTCMPAD_ZERO)
                : (PWM_CHANNEL_GENA_ACTLOAD_ONE  | PWM_CHANNEL_GENA_ACTCMPAD_ZERO);
            generator.address->GENB = IsCenterAligned(config.control.mode)
                ? (PWM_CHANNEL_GENB_ACTCMPBU_ONE | PWM_CHANNEL_GENB_ACTCMPBD_ZERO)
                : (PWM_CHANNEL_GENB_ACTLOAD_ONE  | PWM_CHANNEL_GENB_ACTCMPBD_ZERO);

            if (generator.trigger)
                generator.address->INTEN |= triggerType[static_cast<uint32_t>(*generator.trigger)];

            if (config.deadTime)
                EnableDeadBand(generator);
            else
                generator.address->DBCTL &= ~PWM_CHANNEL_DBCTL_ENABLE;
        }
    }

    void Pwm::EnableDeadBand(Generator& generator) const
    {
        generator.address->DBFALL = config.deadTime->fallInClockCycles;
        generator.address->DBRISE = config.deadTime->riseInClockCycles;
        generator.address->DBCTL |= PWM_CHANNEL_DBCTL_ENABLE;
    }

    void Pwm::EnableGenerator(Generator& generator) const
    {
        generator.address->CTL |= PWM_CHANNEL_CTL_ENABLE;
    }

    void Pwm::DisableGenerator(Generator& generator) const
    {
        generator.address->CTL &= ~PWM_CHANNEL_CTL_ENABLE;
    }

    void Pwm::EnableOutput(const Generator& generator) const
    {
        peripheralPwm[pwmIndex]->ENABLE |= generator.enable;
    }

    void Pwm::DisableOutput(const Generator& generator) const
    {
        peripheralPwm[pwmIndex]->ENABLE &= ~generator.enable;
    }

    void Pwm::SetComparator(Generator& generator, const hal::Percent& dutyCycle) const
    {
        really_assert(dutyCycle.Value() <= 100);

        auto width = GetLoad(generator) * dutyCycle.Value() / 100;

        if (IsCenterAligned(config.control.mode))
            width /= 2;

        auto load = generator.address->LOAD;

        if (width > load)
            width = load;

        if (generator.a)
            generator.address->CMPA = load - width;
        if (generator.b)
            generator.address->CMPB = load - width;

        EnableOutput(generator);
        EnableGenerator(generator);
    }

    void Pwm::Sync() const
    {
        uint32_t ctl = 0;

        for (const auto& gen : generators)
            ctl |= gen.generatorId;

        peripheralPwm[pwmIndex]->CTL = ctl;
    }

    uint32_t Pwm::GetLoad(const Generator& generator) const
    {
        if (IsCenterAligned(config.control.mode))
            return generator.address->LOAD * 2;
        else
            return generator.address->LOAD + 1;
    }

    void Pwm::EnableClock() const
    {
        SYSCTL->RCGCPWM |= (1 << pwmIndex);

        while (!(SYSCTL->PRPWM & (1 << pwmIndex)))
        {
        }
    }

    void Pwm::DisableClock() const
    {
        SYSCTL->RCGCPWM &= ~(1 << pwmIndex);
    }

    void Pwm::HandleGeneratorIrq(GeneratorIndex gen)
    {
        auto* chan = PwmChannel(peripheralPwmArray[pwmIndex], gen);
        const uint32_t active = chan->RIS & chan->INTEN & PWM_CHANNEL_ISC_NORMAL_MASK;
        chan->ISC = active;

        for (uint8_t i = 0; i < static_cast<uint8_t>(normalInterruptBit.size()); ++i)
            if (active & normalInterruptBit[i])
                onNormalInterrupt(NormalEvent{ gen, static_cast<NormalInterruptSource>(i) });
        
    }

    void Pwm::HandleFaultIrq()
    {
        auto* pwm = peripheralPwm[pwmIndex];

        FaultEvent ev{};
        ev.generatorStatus = static_cast<FaultStatus>(pwm->STATUS & 0x0F);
        ev.inputsByGenerator[0] = static_cast<FaultInput>(pwm->_0_FLTSTAT0 & 0x0F);
        ev.inputsByGenerator[1] = static_cast<FaultInput>(pwm->_1_FLTSTAT0 & 0x0F);
        ev.inputsByGenerator[2] = static_cast<FaultInput>(pwm->_2_FLTSTAT0 & 0x0F);
        ev.inputsByGenerator[3] = static_cast<FaultInput>(pwm->_3_FLTSTAT0 & 0x0F);
        ev.comparatorInputsByGenerator[0] = static_cast<FaultInputComparator>(pwm->_0_FLTSTAT1 & 0xFF);
        ev.comparatorInputsByGenerator[1] = static_cast<FaultInputComparator>(pwm->_1_FLTSTAT1 & 0xFF);
        ev.comparatorInputsByGenerator[2] = static_cast<FaultInputComparator>(pwm->_2_FLTSTAT1 & 0xFF);
        ev.comparatorInputsByGenerator[3] = static_cast<FaultInputComparator>(pwm->_3_FLTSTAT1 & 0xFF);

        pwm->ISC = pwm->RIS & (PWM_ISC_INTFAULT3 | PWM_ISC_INTFAULT2 | PWM_ISC_INTFAULT1 | PWM_ISC_INTFAULT0);

        onFault(ev);
    }

    uint16_t Pwm::CalculateDeadTimeCycles(std::chrono::nanoseconds deadTime, Config::ClockDivisor divisor)
    {
        static constexpr std::array<uint32_t, 7> divisorValues = { { 1, 2, 4, 8, 16, 32, 64 } };

        auto divisorValue = divisorValues[static_cast<uint32_t>(divisor)];
        auto pwmClockFreq = GetSystemCoreClock() / static_cast<float>(divisorValue);
        auto deadTimeNs = static_cast<float>(deadTime.count());
        auto cycles = static_cast<uint32_t>(deadTimeNs * pwmClockFreq / 1e9);

        really_assert(cycles <= 4095);

        return static_cast<uint16_t>(cycles);
    }
}
