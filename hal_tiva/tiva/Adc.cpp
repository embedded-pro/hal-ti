#include "hal_tiva/tiva/Adc.hpp"
#include "hal/interfaces/AdcMultiChannel.hpp"
#include "infra/util/EnumCast.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace
{
    extern "C" void AdcSequence0_Handler()
    {
        hal::InterruptTable::Instance().Invoke(ADC0SS0_IRQn);
    }

    extern "C" void AdcSequence1_Handler()
    {
        hal::InterruptTable::Instance().Invoke(ADC0SS1_IRQn);
    }

    extern "C" void AdcSequence2_Handler()
    {
        hal::InterruptTable::Instance().Invoke(ADC0SS2_IRQn);
    }

    extern "C" void AdcSequence3_Handler()
    {
        hal::InterruptTable::Instance().Invoke(ADC0SS3_IRQn);
    }

    constexpr static uint32_t ADC_SSFSTAT0_FULL = 0x00001000;
    constexpr static uint32_t ADC_SSFSTAT0_EMPTY = 0x00000100;
    constexpr static uint32_t ADC_SSFSTAT0_HPTR_M = 0x000000F0;
    constexpr static uint32_t ADC_SSFSTAT0_TPTR_M = 0x0000000F;
    constexpr static uint32_t ADC_SSFSTAT0_HPTR_S = 4;
    constexpr static uint32_t ADC_SSFSTAT0_TPTR_S = 0;

    constexpr static uint32_t ADC_TRIGGER_PROCESSOR = 0x00000000;
    constexpr static uint32_t ADC_TRIGGER_COMP0 = 0x00000001;
    constexpr static uint32_t ADC_TRIGGER_COMP1 = 0x00000002;
    constexpr static uint32_t ADC_TRIGGER_COMP2 = 0x00000003;
    constexpr static uint32_t ADC_TRIGGER_EXTERNAL = 0x00000004;
    constexpr static uint32_t ADC_TRIGGER_TIMER = 0x00000005;
    constexpr static uint32_t ADC_TRIGGER_PWM0 = 0x00000006;
    constexpr static uint32_t ADC_TRIGGER_PWM1 = 0x00000007;
    constexpr static uint32_t ADC_TRIGGER_PWM2 = 0x00000008;
    constexpr static uint32_t ADC_TRIGGER_PWM3 = 0x00000009;
    constexpr static uint32_t ADC_TRIGGER_NEVER = 0x0000000E;
    constexpr static uint32_t ADC_TRIGGER_ALWAYS = 0x0000000F;
    constexpr static uint32_t ADC_TRIGGER_PWM_MOD0 = 0x00000000;
    constexpr static uint32_t ADC_TRIGGER_PWM_MOD1 = 0x00000010;

    constexpr static uint32_t ADC_CTL_IE = 0x00000040;
    constexpr static uint32_t ADC_CTL_END = 0x00000020;

    constexpr static uint32_t ADC_CTL_SHOLD_4 = 0x00000000;
    constexpr static uint32_t ADC_CTL_SHOLD_8 = 0x00200000;
    constexpr static uint32_t ADC_CTL_SHOLD_16 = 0x00400000;
    constexpr static uint32_t ADC_CTL_SHOLD_32 = 0x00600000;
    constexpr static uint32_t ADC_CTL_SHOLD_64 = 0x00800000;
    constexpr static uint32_t ADC_CTL_SHOLD_128 = 0x00A00000;
    constexpr static uint32_t ADC_CTL_SHOLD_256 = 0x00C00000;

    constexpr std::array<uint32_t, 2> peripheralAdcArray = { {
        ADC0_BASE,
        ADC1_BASE,
    } };

    constexpr std::array<IRQn_Type, 8> peripheralIrqAdcArray = { {
        ADC0SS0_IRQn,
        ADC0SS1_IRQn,
        ADC0SS2_IRQn,
        ADC0SS3_IRQn,

        ADC1SS0_IRQn,
        ADC1SS1_IRQn,
        ADC1SS2_IRQn,
        ADC1SS3_IRQn,
    } };

    constexpr std::array<uint32_t, 4> triggerFields = { {
        ADC_TRIGGER_PWM0,
        ADC_TRIGGER_PWM1,
        ADC_TRIGGER_PWM2,
        ADC_TRIGGER_PWM3,
    } };

    constexpr std::array<uint32_t, 7> sampleAndHoldFields = { {
        ADC_CTL_SHOLD_4,
        ADC_CTL_SHOLD_8,
        ADC_CTL_SHOLD_16,
        ADC_CTL_SHOLD_32,
        ADC_CTL_SHOLD_64,
        ADC_CTL_SHOLD_128,
        ADC_CTL_SHOLD_256,
    } };

    const infra::MemoryRange<ADC0_Type* const> peripheralAdc = infra::ReinterpretCastMemoryRange<ADC0_Type* const>(infra::MakeRange(peripheralAdcArray));

    bool IsPwmTrigger(hal::tiva::Adc::Trigger trigger)
    {
        if (trigger == hal::tiva::Adc::Trigger::pwmGenerator0)
            return true;

        if (trigger == hal::tiva::Adc::Trigger::pwmGenerator1)
            return true;

        if (trigger == hal::tiva::Adc::Trigger::pwmGenerator2)
            return true;

        if (trigger == hal::tiva::Adc::Trigger::pwmGenerator3)
            return true;

        return false;
    }

    void SequenceDisable(ADC0_Type& adc, uint8_t sequencer)
    {
        adc.ACTSS &= ~(1 << sequencer);
    }

    void SequenceEnable(ADC0_Type& adc, uint8_t sequencer)
    {
        adc.ACTSS &= ~(1 << sequencer);
    }

    void SequenceConfigure(ADC0_Type& adc, uint8_t sequencer, hal::tiva::Adc::Trigger trigger, uint8_t priority)
    {
        auto triggerParsed = (triggerFields.at(infra::enum_cast(trigger)) & 0xf);
        auto generator = (triggerParsed - ADC_TRIGGER_PWM0) * 8;
        sequencer *= 4;

        adc.EMUX = (adc.EMUX & ~(0xf << sequencer)) | (triggerParsed << sequencer);
        adc.SSPRI = (adc.SSPRI & ~(0xf << sequencer)) | ((priority & 0x3) << sequencer);

        if (IsPwmTrigger(trigger))
            adc.TSSEL = (adc.TSSEL & ~(0x30 << generator)) | ((triggerParsed & 0x30) << generator);
    }

    void SequenceStepConfigure(ADC0_Type& adc, uint8_t sequencer, uint8_t step, uint32_t config)
    {
        uint32_t stepOffset = step * 4;

        volatile uint32_t* SSMUX = &adc.SSMUX0 + (sequencer * 4);
        volatile uint32_t* SSCTL = &adc.SSCTL0 + (sequencer * 4);
        volatile uint32_t* SSEMUX = &adc.SSEMUX0 + (sequencer * 4);
        volatile uint32_t* SSTSH = &adc.SSTSH0 + (sequencer * 4);
        volatile uint32_t* SSOP = &adc.SSOP0 + (sequencer * 4);

        *SSMUX = ((*SSMUX & ~(0x0000000f << stepOffset)) | ((config & 0x0f) << stepOffset));
        *SSEMUX = ((*SSEMUX & ~(0x0000000f << stepOffset)) | (((config & 0xf00) >> 8) << stepOffset));
        *SSCTL = ((*SSCTL & ~(0x0000000f << stepOffset)) | (((config & 0xf0) >> 4) << stepOffset));
        *SSTSH = ((*SSTSH & ~(0x0000000f << stepOffset)) | (((config & 0xf00000) >> 20) << stepOffset));
        *SSOP &= ~(1 << stepOffset);
    }

    void InterruptEnable(ADC0_Type& adc, uint8_t sequencer)
    {
        adc.ISC = 1 << sequencer;
        adc.IM |= 1 << sequencer;
    }

    void InterruptDisable(ADC0_Type& adc, uint8_t sequencer)
    {
        adc.IM &= ~(1 << sequencer);
    }

    bool IsInterruptTriggered(ADC0_Type& adc, uint8_t sequencer)
    {
        return (adc.RIS) & (0x10000 | (1 << sequencer));
    }

    void InterruptClear(ADC0_Type& adc, uint8_t sequencer)
    {
        adc.ISC = 1 << sequencer;
    }

    void DataGet(ADC0_Type& adc, uint8_t sequencer, infra::BoundedVector<uint16_t>& samples)
    {
        volatile uint32_t* SSFSTAT = &adc.SSFSTAT0 + (sequencer * 4);
        volatile uint32_t* SSFIFO = &adc.SSFIFO0 + (sequencer * 4);

        samples.clear();

        while (!((*SSFSTAT) & ADC_SSFSTAT0_EMPTY) && (samples.size() < 8))
            samples.push_back(static_cast<uint16_t>(*SSFIFO));
    }
}

namespace hal::tiva
{
    Adc::Adc(uint8_t adcIndex, uint8_t adcSequencer, infra::MemoryRange<AnalogPin> inputs, const Config& config)
        : ImmediateInterruptHandler(peripheralIrqAdcArray.at(peripheralIrqAdcArray.size() * adcIndex + adcSequencer), [this]()
              {
                  if (IsInterruptTriggered(*peripheralAdc[this->adcIndex], this->adcSequencer))
                  {
                      InterruptClear(*peripheralAdc[this->adcIndex], this->adcSequencer);
                      DataGet(*peripheralAdc[this->adcIndex], this->adcSequencer, buffer);
                  }
              })
        , adcIndex(adcIndex)
        , adcSequencer(adcSequencer)
    {
        really_assert(inputs.size() > 0);

        EnableClock();

        SequenceDisable(*peripheralAdc[adcIndex], adcSequencer);
        SequenceConfigure(*peripheralAdc[adcIndex], adcSequencer, config.trigger, config.priority);

        auto lastChannel = inputs.size() - 1;
        auto sh = sampleAndHoldFields.at(infra::enum_cast(config.sampleAndHold));

        for (std::size_t i = 0; i < inputs.size() - 1; i++)
            SequenceStepConfigure(*peripheralAdc[adcIndex], adcSequencer, i, inputs[i].AdcChannel() | sh);

        SequenceStepConfigure(*peripheralAdc[adcIndex], adcSequencer, lastChannel, inputs[lastChannel].AdcChannel() | sh | ADC_CTL_IE | ADC_CTL_END);
    }

    Adc::~Adc()
    {
        SequenceDisable(*peripheralAdc[adcIndex], adcSequencer);
        InterruptDisable(*peripheralAdc[adcIndex], adcSequencer);
        DisableClock();
    }

    void Adc::Measure(const infra::Function<void(Samples)>& onDone)
    {
        callback = onDone;
        InterruptEnable(*peripheralAdc[adcIndex], adcSequencer);
        SequenceEnable(*peripheralAdc[adcIndex], adcSequencer);
    }

    void Adc::EnableClock()
    {
        SYSCTL->RCGCADC |= 1 << adcIndex;

        while ((SYSCTL->PRADC & (1 << adcIndex)) == 0)
        {
        }
    }

    void Adc::DisableClock()
    {
        SYSCTL->RCGCADC &= ~(1 << adcIndex);
    }
}
