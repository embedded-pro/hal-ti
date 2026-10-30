#ifndef HAL_GPIO_STM_HPP
#define HAL_GPIO_STM_HPP

#include DEVICE_HEADER
#include "GpioFamily.hpp"
#include "hal/cortex_m/InterruptCortex.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "infra/util/HotSwapFunction.hpp"
#include "infra/util/MemoryRange.hpp"
#include <atomic>
#include <cstdint>
#include <optional>

namespace hal::tiva
{
    enum class PinConfigPeripheral : uint8_t
    {
        i2cScl,
        i2cSda,
        uartTx,
        uartRx,
        uartRts,
        uartCts,
        canTx,
        canRx,
        spiClock,
        spiMiso,
        spiMosi,
        spiSlaveSelect,
        usbFsId,
        usbFsDm,
        usbFsDp,
        pwmFault,
        pwmChannel0,
        pwmChannel1,
        pwmChannel2,
        pwmChannel3,
        pwmChannel4,
        pwmChannel5,
        pwmChannel6,
        pwmChannel7,
        qeiIndex,
        qeiPhaseA,
        qeiPhaseB,
        ethernetLed0,
        ethernetLed1,
        ethernetLed2,
        comparatorOutput,
    };

    enum class Type : uint8_t
    {
        adc,
    };

    enum class Port : uint8_t
    {
        A,
        B,
        C,
        D,
        E,
        F,
        G,
        H,
        J,
        K,
        L,
        M,
        N,
        P,
        Q
    };

    enum class Drive : uint8_t
    {
        Up,
        Down,
        OpenDrain,
        None,
        Default = None
    };

    enum class Current : uint8_t
    {
        _2mA,
        _4mA,
        _8mA,
        Default = _2mA
    };

    class GpioPin
        : public hal::GpioPin
    {
    public:
        GpioPin(Port port, uint8_t index, Drive drive = Drive::Default, Current current = Current::Default);

        bool Get() const override;
        void Set(bool value) override;
        bool GetOutputLatch() const override;
        void SetAsInput() override;
        bool IsInput() const override;
        void Config(PinConfigType config) override;
        void Config(PinConfigType config, bool startOutputState) override;
        void ResetConfig() override;
        void EnableInterrupt(const infra::Function<void()>& action, InterruptTrigger trigger, InterruptType type = InterruptType::dispatched) override;
        void DisableInterrupt() override;

        virtual void ConfigAnalog();
        virtual void ConfigPeripheral(PinConfigPeripheral pinConfigType);

        uint32_t AdcChannel() const;

    private:
        Port port;
        uint8_t index;
        Drive drive;
        Current current;
    };

    class DummyPin
        : public GpioPin
    {
    public:
        DummyPin();

        bool Get() const override;
        void Set(bool value) override;
        bool GetOutputLatch() const override;
        void SetAsInput() override;
        bool IsInput() const override;
        void Config(PinConfigType config) override;
        void Config(PinConfigType config, bool startOutputState) override;
        void ResetConfig() override;
        void EnableInterrupt(const infra::Function<void()>& action, InterruptTrigger trigger, InterruptType type = InterruptType::dispatched) override;
        void DisableInterrupt() override;
        void ConfigAnalog() override;
        void ConfigPeripheral(PinConfigPeripheral pinConfigType) override;
    };

    extern DummyPin dummyPin;

    class PeripheralPin
    {
    public:
        PeripheralPin(GpioPin& pin, PinConfigPeripheral pinConfigType);
        PeripheralPin(const PeripheralPin& other) = delete;
        PeripheralPin& operator=(const PeripheralPin& other) = delete;
        ~PeripheralPin();

    private:
        GpioPin& pin;
    };

    class AnalogPin
    {
    public:
        struct Shared
        {};

        static constexpr Shared shared{};

        explicit AnalogPin(GpioPin& pin);
        // Converts a pin that another AnalogPin owns, so one sequence can sample it in more than one step
        AnalogPin(GpioPin& pin, Shared);
        AnalogPin(const AnalogPin& other) = delete;
        AnalogPin& operator=(const AnalogPin& other) = delete;
        ~AnalogPin();

        uint32_t AdcChannel() const;

    private:
        GpioPin& pin;
        bool owner{ true };
    };

    class MultiGpioPin
    {
    public:
        MultiGpioPin(infra::MemoryRange<const std::pair<Port, uint8_t>> table, Drive drive = Drive::Default, Current current = Current::Default);

        void ResetConfig();
        void ConfigAnalog();
        void ConfigPeripheral(PinConfigPeripheral pinConfigType);

    private:
        infra::MemoryRange<const std::pair<Port, uint8_t>> table;
        Drive drive;
        Current current;
    };

    class MultiPeripheralPin
    {
    public:
        MultiPeripheralPin(MultiGpioPin& pins, PinConfigPeripheral pinConfigType);
        MultiPeripheralPin(const MultiPeripheralPin& other) = delete;
        MultiPeripheralPin& operator=(const MultiPeripheralPin& other) = delete;
        ~MultiPeripheralPin();

    private:
        MultiGpioPin& pins;
    };

    class Gpio
        : public infra::InterfaceConnector<Gpio>
    {
    public:
        struct PinPosition
        {
            uint8_t peripheralIndex;
            Port port;
            uint8_t pin;
            uint8_t portControl;
        };

        struct AnalogPinPosition
        {
            Type type;
            Port port;
            uint8_t pin;
            uint8_t channel;
        };

        struct PinoutTable
        {
            PinConfigPeripheral pinConfigType;
            infra::MemoryRange<const PinPosition> pinPositions;
            Drive drive;
            PinConfigType config;
            Current current;
            bool isDigital;
        };

    public:
        Gpio(infra::MemoryRange<const infra::MemoryRange<const Gpio::PinoutTable>> pinoutTable, infra::MemoryRange<const Gpio::AnalogPinPosition> analogTable = infra::MemoryRange<const Gpio::AnalogPinPosition>());

        std::pair<const PinPosition&, const PinoutTable&> GetPeripheralPinConfig(Port port, uint8_t index, PinConfigPeripheral pinConfigType) const;
        uint32_t AdcChannel(Port port, uint8_t index) const;

        void EnableInterrupt(Port port, uint8_t index, const infra::Function<void()>& action, InterruptTrigger trigger, InterruptType type = InterruptType::dispatched);
        void DisableInterrupt(Port port, uint8_t index);

        void ReservePin(Port port, uint8_t index);
        void ClearPinReservation(Port port, uint8_t index);

        static constexpr std::size_t maxInterruptPins{ HAL_TI_GPIO_MAX_INTERRUPT_PINS };
        static_assert(maxInterruptPins <= 8 * family::numberOfPorts, "HAL_TI_GPIO_MAX_INTERRUPT_PINS exceeds the number of GPIO pins");

    private:
        static constexpr uint8_t unassignedPin{ 0xff };

        struct PinInterrupt
        {
            infra::HotSwapFunction<void()> handler;
            std::atomic<uint16_t> pendingEdges{};
            uint8_t pinNumber{ unassignedPin };
            InterruptType type{};
        };

        struct InterruptLine
        {
            std::optional<hal::cortex::ImmediateInterruptHandler> handler;
            int32_t irq{ -1 };
            uint8_t portIndex{};
            uint8_t firstPin{};
            uint8_t endPin{};
        };

        PinInterrupt* FindPinInterrupt(std::size_t pinNumber);
        PinInterrupt& ClaimPinInterrupt(std::size_t pinNumber);
        void ConnectInterruptLine(std::size_t portIndex, uint8_t index);
        void ExtiInterrupt(const InterruptLine& line);
        void Notify(PinInterrupt& pinInterrupt);
        void DispatchPending(PinInterrupt& pinInterrupt);

        infra::MemoryRange<const infra::MemoryRange<const Gpio::PinoutTable>> pinoutTable;
        infra::MemoryRange<const Gpio::AnalogPinPosition> analogTable;

        std::array<uint32_t, 15> assignedPins;

        // Pooled, not one per pin: the TM4C129's 120 pins would cost about 4 KiB of RAM for interrupts few of them use
        std::array<PinInterrupt, maxInterruptPins> pinInterrupts;
        std::array<InterruptLine, maxInterruptPins> interruptLines;
    };
}

#endif
