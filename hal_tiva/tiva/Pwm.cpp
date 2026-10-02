#include "hal_tiva/tiva/Pwm.hpp"
#include "PwmFamily.hpp"
#include "hal_tiva/tiva/Gpio.hpp"
#include "infra/util/EnumCast.hpp"
#include "infra/util/ReallyAssert.hpp"

extern "C" uint32_t SystemCoreClock;

namespace
{
    namespace family = hal::tiva::family;

    using family::numberOfPwms;
    using family::peripheralPwmArray;
    using family::peripheralPwmIrqs;

    extern "C" void Pwm0Generator0_Handler()
    {
        hal::cortex::InterruptTable::Instance().Invoke(PWM0_0_IRQn);
    }

    extern "C" void Pwm0Generator1_Handler()
    {
        hal::cortex::InterruptTable::Instance().Invoke(PWM0_1_IRQn);
    }

    extern "C" void Pwm0Generator2_Handler()
    {
        hal::cortex::InterruptTable::Instance().Invoke(PWM0_2_IRQn);
    }

    extern "C" void Pwm0Generator3_Handler()
    {
        hal::cortex::InterruptTable::Instance().Invoke(PWM0_3_IRQn);
    }

    extern "C" void Pwm0Fault_Handler()
    {
        hal::cortex::InterruptTable::Instance().Invoke(PWM0_FAULT_IRQn);
    }

    const infra::MemoryRange<PWM0_Type* const> peripheralPwm =
        infra::ReinterpretCastMemoryRange<PWM0_Type* const>(infra::MakeRange(peripheralPwmArray));

    constexpr const std::size_t fltstatStride = 0x80 / sizeof(uint32_t); // PWMnFLTSTATx of consecutive generators
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
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTLOAD_ZERO  = 0x00000008;
    constexpr const uint32_t PWM_CHANNEL_GENA_ACTLOAD_ONE   = 0x0000000C;

    constexpr const uint32_t PWM_CHANNEL_GENB_ACTCMPBU_ONE  = 0x00000300;
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTCMPBD_ZERO = 0x00000800;
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTLOAD_ZERO  = 0x00000008;
    constexpr const uint32_t PWM_CHANNEL_GENB_ACTLOAD_ONE   = 0x0000000C;

    constexpr const uint32_t PWM_CHANNEL_DBCTL_ENABLE = 0x00000001;

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

    constexpr const std::array<uint32_t, 4> generatorIntEnBit = { {
        0x00000001u,
        0x00000002u,
        0x00000004u,
        0x00000008u,
    } };

    float GetSystemCoreClock()
    {
        return static_cast<float>(SystemCoreClock);
    }

    uint32_t ToPeriod(PWM0_Type* const pwmBase, const hal::Hertz& baseFrequency)
    {
        auto pwmClock = SystemCoreClock / family::GetClockDivisor(pwmBase);
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

    Pwm::GeneratorInterruptSlot::GeneratorInterruptSlot(Pwm& owner, int32_t irq, hal::cortex::InterruptPriority priority, GeneratorIndex gen)
        : owner(owner)
        , gen(gen)
        , handler(irq, priority, [this]()
              {
                  this->owner.HandleGeneratorIrq(this->gen);
              })
    {}

    Pwm::FaultInterruptSlot::FaultInterruptSlot(Pwm& owner, int32_t irq, hal::cortex::InterruptPriority priority)
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
        for (auto& gen : generators)
            gen.address->INTEN &= ~PWM_CHANNEL_ISC_NORMAL_MASK;

        peripheralPwm[pwmIndex]->INTEN = 0;

        for (auto& h : generatorHandlers)
            h.reset();
        faultHandler.reset();

        DisableGenerators();
        peripheralPwm[pwmIndex]->FAULT &= ~faultOutputs;

        uint32_t invertMask = 0;
        for (const auto& gen : generators)
        {
            if (config.channelAInverted)
                invertMask |= gen.enable & 0x55u;
            if (config.channelBInverted)
                invertMask |= gen.enable & 0xAAu;
        }
        peripheralPwm[pwmIndex]->INVERT &= ~invertMask;

        DisableClock();
    }

    void Pwm::Initialize()
    {
        EnableClock();
        family::SetClockDivisor(peripheralPwm[pwmIndex], config.clockDivisor);

        for (auto& gen : generators)
            GeneratorConfiguration(gen);

        ConfigureInvert();

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
            peripheralPwm[pwmIndex]->INTEN |= generatorIntEnBit[genIdx];

            if (!generatorHandlers[genIdx].has_value())
            {
                channel->ISC = PWM_CHANNEL_ISC_NORMAL_MASK;
                NVIC_ClearPendingIRQ(static_cast<IRQn_Type>(peripheralPwmIrqs[pwmIndex].generatorIrqs[genIdx]));
                generatorHandlers[genIdx].emplace(*this, peripheralPwmIrqs[pwmIndex].generatorIrqs[genIdx], interruptConfig.priority, normalSource.generator);
            }
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

            configuredFaultInterrupts |= faultIntEnBit[genIdx];
            for (const auto& gen : generators)
                if (gen.generatorId == generatorIntEnBit[genIdx])
                    faultOutputs |= gen.enable;
            hasFault = true;
        }

        if (hasFault)
        {
            peripheralPwm[pwmIndex]->FAULTVAL &= ~faultOutputs;
            peripheralPwm[pwmIndex]->FAULT |= faultOutputs;
            peripheralPwm[pwmIndex]->ISC = PWM_ISC_INTFAULT3 | PWM_ISC_INTFAULT2 | PWM_ISC_INTFAULT1 | PWM_ISC_INTFAULT0;
            peripheralPwm[pwmIndex]->INTEN |= configuredFaultInterrupts;
            NVIC_ClearPendingIRQ(static_cast<IRQn_Type>(peripheralPwmIrqs[pwmIndex].faultIrq));
            faultHandler.emplace(*this, peripheralPwmIrqs[pwmIndex].faultIrq, interruptConfig.priority);
        }
    }

    void Pwm::SetBaseFrequency(hal::Hertz baseFrequency)
    {
        auto load = ToPeriod(peripheralPwm[pwmIndex], baseFrequency);
        load = IsCenterAligned(config.control.mode) ? load / 2 : load - 1;
        really_assert(load > 0 && load <= 0xffff);

        for (auto& gen : generators)
            if (gen.a || gen.b)
                gen.address->LOAD = load;

        // The comparators count against the old period; recompute them so a running generator keeps its duty cycle
        for (auto& gen : generators)
            if (gen.duty && (gen.address->CTL & PWM_CHANNEL_CTL_ENABLE) != 0)
                SetComparator(gen, *gen.duty);

        Sync();
    }

    void Pwm::Start(hal::DutyCycle dutyCycle)
    {
        really_assert(generators.size() >= 1);
        const uint32_t running = RunningGenerators();

        for (auto& gen : generators)
            SetComparator(gen, dutyCycle);

        Sync();
        SynchronizeCounters(running);
    }

    void Pwm::Start(hal::DutyCycle dutyCycle1, hal::DutyCycle dutyCycle2)
    {
        really_assert(generators.size() == 2);
        const uint32_t running = RunningGenerators();

        SetComparator(generators[0], dutyCycle1);
        SetComparator(generators[1], dutyCycle2);

        Sync();
        SynchronizeCounters(running);
    }

    void Pwm::Start(hal::DutyCycle dutyCycle1, hal::DutyCycle dutyCycle2, hal::DutyCycle dutyCycle3)
    {
        really_assert(generators.size() == 3);
        const uint32_t running = RunningGenerators();

        SetComparator(generators[0], dutyCycle1);
        SetComparator(generators[1], dutyCycle2);
        SetComparator(generators[2], dutyCycle3);

        Sync();
        SynchronizeCounters(running);
    }

    void Pwm::Start(hal::DutyCycle dutyCycle1, hal::DutyCycle dutyCycle2, hal::DutyCycle dutyCycle3, hal::DutyCycle dutyCycle4)
    {
        really_assert(generators.size() == 4);
        const uint32_t running = RunningGenerators();

        SetComparator(generators[0], dutyCycle1);
        SetComparator(generators[1], dutyCycle2);
        SetComparator(generators[2], dutyCycle3);
        SetComparator(generators[3], dutyCycle4);

        Sync();
        SynchronizeCounters(running);
    }

    void Pwm::Stop()
    {
        DisableGenerators();
        RearmFault();
    }

    void Pwm::DisableGenerators()
    {
        for (auto& gen : generators)
        {
            DisableGenerator(gen);
            DisableOutput(gen);
        }

        Sync();
    }

    void Pwm::RearmFault() const
    {
        if (configuredFaultInterrupts == 0)
            return;

        auto* pwm = peripheralPwm[pwmIndex];
        for (uint8_t gen = 0; gen < static_cast<uint8_t>(faultIntEnBit.size()); ++gen)
        {
            if ((configuredFaultInterrupts & faultIntEnBit[gen]) == 0)
                continue;

            // FLTSTAT is write-one-to-clear in latch mode; CMSIS declares it read-only
            *const_cast<volatile uint32_t*>(&pwm->_0_FLTSTAT0 + gen * fltstatStride) = 0x0F;
            *const_cast<volatile uint32_t*>(&pwm->_0_FLTSTAT1 + gen * fltstatStride) = 0xFF;
        }

        pwm->ISC = configuredFaultInterrupts;
        NVIC_ClearPendingIRQ(static_cast<IRQn_Type>(peripheralPwmIrqs[pwmIndex].faultIrq));
        pwm->INTEN |= configuredFaultInterrupts;
    }

    void Pwm::GeneratorConfiguration(Generator& generator) const
    {
        if (generator.a || generator.b)
        {
            generator.address->CTL |= config.control.Value();
            if (generator.a)
                generator.address->GENA = IsCenterAligned(config.control.mode)
                    ? (PWM_CHANNEL_GENA_ACTCMPAU_ONE | PWM_CHANNEL_GENA_ACTCMPAD_ZERO)
                    : (PWM_CHANNEL_GENA_ACTLOAD_ONE  | PWM_CHANNEL_GENA_ACTCMPAD_ZERO);
            if (generator.b)
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

    void Pwm::SetComparator(Generator& generator, const hal::DutyCycle& dutyCycle) const
    {
        really_assert(dutyCycle.IsValid());
        generator.duty = dutyCycle;

        auto load = generator.address->LOAD;
        auto width = static_cast<uint32_t>(IsCenterAligned(config.control.mode) ? dutyCycle.ToCounts(load) : dutyCycle.ToCounts(GetLoad(generator)));

        if (width > load)
            width = load;

        if (width == 0)
        {
            if (generator.a)
                generator.address->GENA = PWM_CHANNEL_GENA_ACTLOAD_ZERO;
            if (generator.b)
                generator.address->GENB = PWM_CHANNEL_GENB_ACTLOAD_ZERO;
        }
        else if (width == load)
        {
            if (generator.a)
                generator.address->GENA = PWM_CHANNEL_GENA_ACTLOAD_ONE;
            if (generator.b)
                generator.address->GENB = PWM_CHANNEL_GENB_ACTLOAD_ONE;
        }
        else
        {
            if (generator.a)
                generator.address->GENA = IsCenterAligned(config.control.mode)
                    ? (PWM_CHANNEL_GENA_ACTCMPAU_ONE | PWM_CHANNEL_GENA_ACTCMPAD_ZERO)
                    : (PWM_CHANNEL_GENA_ACTLOAD_ONE  | PWM_CHANNEL_GENA_ACTCMPAD_ZERO);
            if (generator.b)
                generator.address->GENB = IsCenterAligned(config.control.mode)
                    ? (PWM_CHANNEL_GENB_ACTCMPBU_ONE | PWM_CHANNEL_GENB_ACTCMPBD_ZERO)
                    : (PWM_CHANNEL_GENB_ACTLOAD_ONE  | PWM_CHANNEL_GENB_ACTCMPBD_ZERO);

            if (generator.a)
                generator.address->CMPA = load - width;
            if (generator.b)
                generator.address->CMPB = load - width;
        }

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

    uint32_t Pwm::RunningGenerators() const
    {
        uint32_t running = 0;

        for (const auto& gen : generators)
            if ((gen.address->CTL & PWM_CHANNEL_CTL_ENABLE) != 0)
                running |= gen.generatorId;

        return running;
    }

    // Each counter starts when its generator is enabled, so newly started generators are restarted together to keep their edges aligned
    void Pwm::SynchronizeCounters(uint32_t runningBefore) const
    {
        uint32_t all = 0;

        for (const auto& gen : generators)
            all |= gen.generatorId;

        if (runningBefore != all)
            peripheralPwm[pwmIndex]->SYNC = all;
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

    void Pwm::ConfigureInvert() const
    {
        uint32_t invertMask = 0;
        for (const auto& gen : generators)
        {
            if (config.channelAInverted)
                invertMask |= gen.enable & 0x55u;
            if (config.channelBInverted)
                invertMask |= gen.enable & 0xAAu;
        }
        peripheralPwm[pwmIndex]->INVERT |= invertMask;
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

    // One event per fault: the fault interrupt stays masked until Stop(), so a fault that persists or repeats cannot flood the
    // event loop while the hardware holds the outputs at their inactive level
    void Pwm::HandleFaultIrq()
    {
        auto* pwm = peripheralPwm[pwmIndex];

        pwm->INTEN &= ~configuredFaultInterrupts;
        pwm->ISC = configuredFaultInterrupts;

        const uint32_t configuredGenerators = configuredFaultInterrupts >> 16;
        uint32_t generators = pwm->STATUS & configuredGenerators;
        if (generators == 0)
            generators = configuredGenerators;

        FaultEvent ev{};
        ev.generatorStatus = static_cast<FaultStatus>(generators);

        for (uint8_t gen = 0; gen < static_cast<uint8_t>(faultIntEnBit.size()); ++gen)
        {
            if ((generators & (1u << gen)) == 0)
                continue;

            auto* channel = PwmChannel(peripheralPwmArray[pwmIndex], static_cast<GeneratorIndex>(gen));
            uint32_t inputs = *(&pwm->_0_FLTSTAT0 + gen * fltstatStride) & 0x0F;
            uint32_t comparators = *(&pwm->_0_FLTSTAT1 + gen * fltstatStride) & 0xFF;

            if (inputs == 0 && comparators == 0)
            {
                inputs = channel->FLTSRC0 & 0x0F;
                comparators = channel->FLTSRC1 & 0xFF;
            }

            ev.inputsByGenerator[gen] = static_cast<FaultInput>(inputs);
            ev.comparatorInputsByGenerator[gen] = static_cast<FaultInputComparator>(comparators);
        }

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
