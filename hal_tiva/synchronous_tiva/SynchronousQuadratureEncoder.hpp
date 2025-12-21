#ifndef HAL_QUADRATURE_ENCODER_TIVA_HPP
#define HAL_QUADRATURE_ENCODER_TIVA_HPP

#include "hal/synchronous_interfaces/SynchronousQuadratureEncoder.hpp"
#include "hal_tiva/cortex/InterruptCortex.hpp"
#include "hal_tiva/tiva/Gpio.hpp"
#include "infra/util/Function.hpp"

namespace hal::tiva
{
    class QuadratureEncoder
        : public hal::SynchronousQuadratureEncoder
    {
    public:
        struct Config
        {
            enum class ResetMode
            {
                onMaxPosition,
                onIndexPulse,
            };

            enum class CaptureMode
            {
                onlyPhaseA,
                phaseAandPhaseB,
            };

            enum class SignalMode
            {
                quadrature,
                clockAndDirection,
            };

            constexpr Config()
            {}

            Config(uint32_t aResolution, uint32_t anOffset, bool anInvertPhaseA, bool anInvertPhaseB, bool anInvertIndex, ResetMode aResetMode, CaptureMode aCaptureMode, SignalMode aSignalMode)
                : resolution(aResolution)
                , offset(anOffset)
                , invertPhaseA(anInvertPhaseA)
                , invertPhaseB(anInvertPhaseB)
                , invertIndex(anInvertIndex)
                , resetMode(aResetMode)
                , captureMode(aCaptureMode)
                , signalMode(aSignalMode)
            {}

            uint32_t resolution = 1024;
            uint32_t offset = 0;
            bool invertPhaseA = false;
            bool invertPhaseB = false;
            bool invertIndex = false;
            ResetMode resetMode = ResetMode::onMaxPosition;
            CaptureMode captureMode = CaptureMode::phaseAandPhaseB;
            SignalMode signalMode = SignalMode::quadrature;
        };

        QuadratureEncoder(uint8_t aQeiIndex, GpioPin& phaseA = dummyPin, GpioPin& phaseB = dummyPin, GpioPin& index = dummyPin, const Config& config = Config());
        ~QuadratureEncoder();

        uint32_t Position() override;
        uint32_t Resolution() override;
        QuadratureEncoder::MotionDirection Direction() override;
        uint32_t Speed() override;

    private:
        void EnableClock();
        void DisableClock();

    private:
        uint8_t qeiIndex;
        PeripheralPin phaseA;
        PeripheralPin phaseB;
        PeripheralPin index;

        infra::MemoryRange<QEI0_Type* const> qeiArray;
        infra::MemoryRange<IRQn_Type const> irqArray;

        infra::Function<void(MotionDirection)> onDirectionChange;
        infra::Optional<ImmediateInterruptHandler> qeiInterruptRegistration;
    };
}

#endif
