#include "hal_tiva/synchronous_tiva/SynchronousAnalogComparator.hpp"
#include "infra/util/ReallyAssert.hpp"
#include DEVICE_HEADER

namespace
{
    constexpr uint32_t AcctlAsrcpShift  = 9;
    constexpr uint32_t AcctlAsrcpMask   = 0x3u << AcctlAsrcpShift;
    constexpr uint32_t AcctlTslval      = 1u << 7;
    constexpr uint32_t AcctlTsenShift   = 5;
    constexpr uint32_t AcctlTsenMask    = 0x3u << AcctlTsenShift;
    constexpr uint32_t AcctlCinv        = 1u << 1;
    constexpr uint32_t AcctlToen        = 1u << 0;

    constexpr uint32_t AcstatOval       = 1u << 1;

    constexpr uint32_t AcrefctlEn       = 1u << 9;
    constexpr uint32_t AcrefctlRng      = 1u << 8;
    constexpr uint32_t AcrefctlVrefMask = 0x0Fu;
    constexpr uint32_t AcrefctlCfgMask  = AcrefctlEn | AcrefctlRng | AcrefctlVrefMask;
}

namespace hal::tiva
{
    SynchronousAnalogComparator::SynchronousAnalogComparator(uint8_t aIndex, GpioPin& vinPositive, GpioPin& vinNegative, GpioPin& outputPin, const Config& aConfig)
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

        AnalogComparator::EnableAcmpClock();
        ConfigureReference();
        ConfigureControl();
        COMP->ACMIS = (1u << index);
    }

    SynchronousAnalogComparator::~SynchronousAnalogComparator()
    {
        Acctl() = 0;
        outputPeripheralPin.reset();
        vinNegativePin.reset();
        vinPositivePin.reset();
        AnalogComparator::DisableAcmpClockIfLastInstance();
    }

    bool SynchronousAnalogComparator::GetOutput() const
    {
        return (Acstat() & AcstatOval) != 0;
    }

    void SynchronousAnalogComparator::ConfigureReference() const
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

    void SynchronousAnalogComparator::ConfigureControl() const
    {
        Acctl() = ComputeAcctl();
    }

    uint32_t SynchronousAnalogComparator::ComputeAcctl() const
    {
        uint32_t val = (static_cast<uint32_t>(config.positiveSource) << AcctlAsrcpShift) & AcctlAsrcpMask;

        if (config.triggerEnabled)
        {
            val |= (static_cast<uint32_t>(config.triggerSense) << AcctlTsenShift) & AcctlTsenMask;
            if (config.triggerLevelHigh)
                val |= AcctlTslval;
        }

        if (config.invertOutput)
            val |= AcctlCinv;
        if (config.outputToPin)
            val |= AcctlToen;

        return val;
    }

    volatile uint32_t& SynchronousAnalogComparator::Acctl() const
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

    volatile uint32_t& SynchronousAnalogComparator::Acstat() const
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
}
