#include "hal_tiva/synchronous_tiva/SynchronousAdc.hpp"
#include "infra/util/EnumCast.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace
{
    constexpr static size_t sequencerOffset = 8;

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

    void SequenceDisable(ADC0_Type& adc, uint8_t sequencer)
    {
        adc.ACTSS &= ~(1 << sequencer);
    }

    void SequenceEnable(ADC0_Type& adc, uint8_t sequencer)
    {
        adc.ACTSS |= (1 << sequencer);
    }

    void SequenceConfigure(ADC0_Type& adc, uint8_t sequencer, uint8_t priority)
    {
        static constexpr uint32_t processorTrigger = 0x0;

        auto triggerParsed = processorTrigger & 0xf;
        auto sequencerShift = sequencer * 4;
        auto tsselShift = 4 + (sequencer * sequencerOffset);

        adc.EMUX = (adc.EMUX & ~(0xf << sequencerShift)) | (triggerParsed << sequencerShift);
        adc.SSPRI = (adc.SSPRI & ~(0xf << sequencerShift)) | ((priority & 0x3) << sequencerShift);
    }

    void SequenceStepConfigure(ADC0_Type& adc, uint8_t sequencer, uint8_t step, uint32_t config)
    {
        uint32_t stepOffset = step * 4;

        volatile uint32_t* SSMUX = &adc.SSMUX0 + (sequencer * sequencerOffset);
        volatile uint32_t* SSCTL = &adc.SSCTL0 + (sequencer * sequencerOffset);
        volatile uint32_t* SSEMUX = &adc.SSEMUX0 + (sequencer * sequencerOffset);
        volatile uint32_t* SSTSH = &adc.SSTSH0 + (sequencer * sequencerOffset);
        volatile uint32_t* SSOP = &adc.SSOP0 + (sequencer * sequencerOffset);

        *SSMUX = ((*SSMUX & ~(0x0000000f << stepOffset)) | ((config & 0x0f) << stepOffset));
        *SSEMUX = ((*SSEMUX & ~(0x0000000f << stepOffset)) | (((config & 0xf00) >> 8) << stepOffset));
        *SSCTL = ((*SSCTL & ~(0x0000000f << stepOffset)) | (((config & 0xf0) >> 4) << stepOffset));
        *SSTSH = ((*SSTSH & ~(0x0000000f << stepOffset)) | (((config & 0xf00000) >> 20) << stepOffset));
        *SSOP &= ~(1 << stepOffset);
    }

    void SequenceOversampling(ADC0_Type& adc, uint8_t oversampling)
    {
        adc.SAC = oversampling;
    }

    void GenerateProcessorTrigger(ADC0_Type& adc, uint8_t sequencer)
    {
        adc.PSSI = (1 << sequencer);
    }

    bool IsInterruptTriggered(ADC0_Type& adc, uint8_t sequencer)
    {
        return (adc.RIS) & (0x10000 | (1 << sequencer));
    }

    void InterruptClear(ADC0_Type& adc, uint8_t sequencer)
    {
        adc.ISC = 1 << sequencer;
    }

    void DataGet(ADC0_Type& adc, uint8_t sequencer, infra::BoundedVector<uint16_t>& samples, std::size_t numberOfSamples)
    {
        volatile uint32_t* SSFSTAT = &adc.SSFSTAT0 + (sequencer * sequencerOffset);
        volatile uint32_t* SSFIFO = &adc.SSFIFO0 + (sequencer * sequencerOffset);

        samples.clear();

        while (!((*SSFSTAT) & ADC_SSFSTAT0_EMPTY) && numberOfSamples--)
            samples.push_back(static_cast<uint16_t>(*SSFIFO));
    }
}

namespace hal::tiva
{
    SynchronousAdc::SynchronousAdc(uint8_t adcIndex, uint8_t adcSequencer, infra::MemoryRange<AnalogPin> inputs, const Config& config)
        : adcIndex(adcIndex)
        , adcSequencer(adcSequencer)
        , numberOfInputs(inputs.size())
    {
        really_assert(inputs.size() > 0);

        EnableClock();

        SequenceDisable(*peripheralAdc[adcIndex], adcSequencer);
        SequenceConfigure(*peripheralAdc[adcIndex], adcSequencer, static_cast<uint8_t>(config.priority));

        auto lastChannel = inputs.size() - 1;
        auto sh = sampleAndHoldFields.at(infra::enum_cast(config.sampleAndHold));

        for (std::size_t i = 0; i < inputs.size() - 1; i++)
            SequenceStepConfigure(*peripheralAdc[adcIndex], adcSequencer, i, inputs[i].AdcChannel() | sh);

        SequenceStepConfigure(*peripheralAdc[adcIndex], adcSequencer, lastChannel, inputs[lastChannel].AdcChannel() | sh | ADC_CTL_IE | ADC_CTL_END);

        if (config.oversampling)
            SequenceOversampling(*peripheralAdc[adcIndex], infra::enum_cast(*config.oversampling));
    }

    SynchronousAdc::~SynchronousAdc()
    {
        SequenceDisable(*peripheralAdc[adcIndex], adcSequencer);
        DisableClock();
    }

    SynchronousAdc::Samples SynchronousAdc::Measure(std::size_t)
    {
        buffer.clear();

        SequenceEnable(*peripheralAdc[adcIndex], adcSequencer);
        InterruptClear(*peripheralAdc[adcIndex], adcSequencer);

        peripheralAdc[adcIndex]->PSSI = (1 << adcSequencer);

        while (!IsInterruptTriggered(*peripheralAdc[adcIndex], adcSequencer))
        {
        }

        InterruptClear(*peripheralAdc[adcIndex], adcSequencer);
        DataGet(*peripheralAdc[adcIndex], adcSequencer, buffer, numberOfInputs);
        SequenceDisable(*peripheralAdc[adcIndex], adcSequencer);

        return infra::MakeRange(buffer);
    }

    void SynchronousAdc::EnableClock() const
    {
        SYSCTL->RCGCADC |= 1 << adcIndex;

        while ((SYSCTL->PRADC & (1 << adcIndex)) == 0)
        {
        }
    }

    void SynchronousAdc::DisableClock() const
    {
        SYSCTL->RCGCADC &= ~(1 << adcIndex);
    }
}
