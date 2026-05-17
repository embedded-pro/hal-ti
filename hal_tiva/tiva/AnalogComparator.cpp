#include "hal_tiva/tiva/AnalogComparator.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace
{
    extern "C" void Comp0_Handler()
    {
        hal::InterruptTable::Instance().Invoke(COMP0_IRQn);
    }

    extern "C" void Comp1_Handler()
    {
        hal::InterruptTable::Instance().Invoke(COMP1_IRQn);
    }

#if defined(TM4C129)
    extern "C" void Comp2_Handler()
    {
        hal::InterruptTable::Instance().Invoke(COMP2_IRQn);
    }
#endif

#if defined(TM4C129)
    constexpr std::array<IRQn_Type, 3> peripheralIrqComp = { {
        COMP0_IRQn,
        COMP1_IRQn,
        COMP2_IRQn,
    } };
#else
    constexpr std::array<IRQn_Type, 2> peripheralIrqComp = { {
        COMP0_IRQn,
        COMP1_IRQn,
    } };
#endif

    constexpr uint32_t AcctlAsrcpShift   = 9;
    constexpr uint32_t AcctlAsrcpMask    = 0x3u << AcctlAsrcpShift;
    constexpr uint32_t AcctlTslval       = 1u << 7;
    constexpr uint32_t AcctlTsenShift    = 5;
    constexpr uint32_t AcctlTsenMask     = 0x3u << AcctlTsenShift;
    constexpr uint32_t AcctlIslval       = 1u << 4;
    constexpr uint32_t AcctlIsenShift    = 2;
    constexpr uint32_t AcctlIsenMask     = 0x3u << AcctlIsenShift;
    constexpr uint32_t AcctlCinv         = 1u << 1;
    constexpr uint32_t AcctlToen         = 1u << 0;

    constexpr uint32_t AcstatOval        = 1u << 1;

    constexpr uint32_t AcrefctlEn        = 1u << 9;
    constexpr uint32_t AcrefctlRng       = 1u << 8;
    constexpr uint32_t AcrefctlVrefMask  = 0x0Fu;
    constexpr uint32_t AcrefctlCfgMask   = AcrefctlEn | AcrefctlRng | AcrefctlVrefMask;

    volatile uint8_t instanceCount = 0;

    hal::InterruptTrigger ToInterruptTrigger(hal::tiva::AnalogComparator::InterruptSense sense, bool levelHigh)
    {
        switch (sense)
        {
            case hal::tiva::AnalogComparator::InterruptSense::rising:
                return hal::InterruptTrigger::risingEdge;
            case hal::tiva::AnalogComparator::InterruptSense::falling:
                return hal::InterruptTrigger::fallingEdge;
            case hal::tiva::AnalogComparator::InterruptSense::both:
            case hal::tiva::AnalogComparator::InterruptSense::level:
            default:
                return hal::InterruptTrigger::bothEdges;
        }
    }

    hal::tiva::AnalogComparator::InterruptSense SenseFromTrigger(hal::InterruptTrigger trigger)
    {
        switch (trigger)
        {
            case hal::InterruptTrigger::risingEdge:
                return hal::tiva::AnalogComparator::InterruptSense::rising;
            case hal::InterruptTrigger::fallingEdge:
                return hal::tiva::AnalogComparator::InterruptSense::falling;
            case hal::InterruptTrigger::bothEdges:
            default:
                return hal::tiva::AnalogComparator::InterruptSense::both;
        }
    }

    bool LevelHighFromTrigger(hal::InterruptTrigger)
    {
        return true;
    }
}

namespace hal::tiva
{
    AnalogComparator::AnalogComparator(uint8_t aIndex, GpioPin& vinPositive, GpioPin& vinNegative, GpioPin& outputPin, const Config& aConfig)
        : index(aIndex)
        , config(aConfig)
    {
        really_assert(config.routeToPwmFault.has_value() ? config.triggerEnabled : true);

        if (&vinPositive != &dummyPin)
            vinPositivePin.emplace(vinPositive);
        if (&vinNegative != &dummyPin)
            vinNegativePin.emplace(vinNegative);
        if (config.outputToPin && &outputPin != &dummyPin)
            outputPeripheralPin.emplace(outputPin, PinConfigPeripheral::comparatorOutput);

        EnableAcmpClock();
        ConfigureReference();
        ConfigureControl(std::nullopt);
        COMP->ACMIS = (1u << index);
        irqHandler.emplace(peripheralIrqComp[index], config.priority, [this]() { HandleIrq(); });
    }

    AnalogComparator::~AnalogComparator()
    {
        irqHandler.reset();
        COMP->ACINTEN &= ~(1u << index);
        Acctl() = 0;
        outputPeripheralPin.reset();
        vinNegativePin.reset();
        vinPositivePin.reset();
        DisableAcmpClockIfLastInstance();
    }

    void AnalogComparator::Enable(const infra::Function<void(bool output)>& aOnOutputChanged, InterruptTrigger trigger)
    {
        onOutputChanged = aOnOutputChanged;
        const InterruptSense isen = SenseFromTrigger(trigger);
        const uint32_t isenBits = (static_cast<uint32_t>(isen) << AcctlIsenShift) & AcctlIsenMask;
        Acctl() = (Acctl() & ~(AcctlIsenMask | AcctlIslval)) | isenBits;
        COMP->ACMIS = (1u << index);
        COMP->ACINTEN |= (1u << index);
    }

    void AnalogComparator::Disable()
    {
        COMP->ACINTEN &= ~(1u << index);
        COMP->ACMIS = (1u << index);
        onOutputChanged = nullptr;
    }

    bool AnalogComparator::GetOutput() const
    {
        return (Acstat() & AcstatOval) != 0;
    }

    void AnalogComparator::EnableAcmpClock()
    {
        if (instanceCount == 0)
        {
            SYSCTL->RCGCACMP |= 1u;
            while ((SYSCTL->PRACMP & 1u) == 0)
            {}
        }
        ++instanceCount;
    }

    void AnalogComparator::DisableAcmpClockIfLastInstance()
    {
        --instanceCount;
        if (instanceCount == 0)
            SYSCTL->RCGCACMP &= ~1u;
    }

    void AnalogComparator::ConfigureReference() const
    {
        if (!config.internalReference.has_value())
            return;

        const auto& ref = *config.internalReference;
        really_assert(ref.step <= 15);
        uint32_t newVal = AcrefctlEn
                        | (static_cast<uint32_t>(ref.range) << 8)
                        | (ref.step & AcrefctlVrefMask);

        uint32_t existing = COMP->ACREFCTL & AcrefctlCfgMask;
        if ((existing & AcrefctlEn) != 0)
            really_assert(existing == (newVal & AcrefctlCfgMask));
        else
            COMP->ACREFCTL = newVal;
    }

    void AnalogComparator::ConfigureControl(std::optional<InterruptSense> overrideSense) const
    {
        InterruptSense isen = overrideSense.has_value() ? *overrideSense : config.interruptSense;
        bool islval = overrideSense.has_value() ? LevelHighFromTrigger(ToInterruptTrigger(isen, config.interruptLevelHigh))
                                                : config.interruptLevelHigh;
        Acctl() = ComputeAcctl(isen, islval);
    }

    uint32_t AnalogComparator::ComputeAcctl(InterruptSense isen, bool islval) const
    {
        uint32_t val = (static_cast<uint32_t>(config.positiveSource) << AcctlAsrcpShift) & AcctlAsrcpMask;

        if (config.triggerEnabled)
        {
            val |= (static_cast<uint32_t>(config.triggerSense) << AcctlTsenShift) & AcctlTsenMask;
            if (config.triggerLevelHigh)
                val |= AcctlTslval;
        }

        val |= (static_cast<uint32_t>(isen) << AcctlIsenShift) & AcctlIsenMask;
        if (islval)
            val |= AcctlIslval;

        if (config.invertOutput)
            val |= AcctlCinv;
        if (config.outputToPin)
            val |= AcctlToen;

        return val;
    }

    volatile uint32_t& AnalogComparator::Acctl() const
    {
        switch (index)
        {
            case 0:
                return COMP->ACCTL0;
            case 1:
                return COMP->ACCTL1;
#if defined(TM4C129)
            case 2:
                return COMP->ACCTL2;
#endif
            default:
                really_assert(false);
                return COMP->ACCTL0;
        }
    }

    volatile uint32_t& AnalogComparator::Acstat() const
    {
        switch (index)
        {
            case 0:
                return COMP->ACSTAT0;
            case 1:
                return COMP->ACSTAT1;
#if defined(TM4C129)
            case 2:
                return COMP->ACSTAT2;
#endif
            default:
                really_assert(false);
                return COMP->ACSTAT0;
        }
    }

    void AnalogComparator::HandleIrq()
    {
        bool output = (Acstat() & AcstatOval) != 0;
        COMP->ACMIS = (1u << index);
        if (onOutputChanged)
            onOutputChanged(output);
    }
}
