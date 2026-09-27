#include "hal_tiva/tiva/Gpio.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/BitLogic.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <array>

namespace hal::tiva
{
    namespace
    {
        using Peripheral = hal::tiva::family::GpioPortEntry;
        using hal::tiva::family::perPinIrqs;
        using hal::tiva::family::portAndRcgc;

        struct PushPull
        {
            bool odr;
            bool pur;
            bool pdr;
        };

        struct Mode
        {
            bool den;
            bool amsel;
            bool dir;
        };

        struct CurrentDrive
        {
            bool _2mA;
            bool _4mA;
            bool _8mA;
        };

        struct Interrupt
        {
            bool ibe;
            bool is;
            bool iev;
        };

        struct Pin
        {
            uint32_t mask;
            uint32_t bits;
        };

        const std::array<PushPull, 4> pushPullTiva{ {
            { false, true, false },  /* up */
            { false, false, true },  /* down */
            { true, false, false },  /* open drain */
            { false, false, false }, /* disable */
        } };

        const std::array<Mode, 4> modeTiva{ {
            { true, false, false },  /* in */
            { true, false, true },   /* out */
            { false, false, false }, /* alternate */
            { false, true, true },   /* analog */
        } };

        const std::array<CurrentDrive, 4> currentDriveTiva{ {
            { true, false, false }, /* 2mA */
            { false, true, false }, /* 4mA */
            { false, false, true }, /* 8mA */
            { true, false, false }, /* 2mA */
        } };

        const std::array<Interrupt, 4> interruptTiva{ {
            { false, false, true },  /* rising */
            { false, false, false }, /* falling */
            { true, false, false },  /* both */
            { false, false, false }, /* none */
        } };

        const std::array<Pin, 8> pinTiva{ {
            { 0xFFFFFFF0, 0 },
            { 0xFFFFFF0F, 4 },
            { 0xFFFFF0FF, 8 },
            { 0xFFFF0FFF, 12 },
            { 0xFFF0FFFF, 16 },
            { 0xFF0FFFFF, 20 },
            { 0xF0FFFFFF, 24 },
            { 0x0FFFFFFF, 28 },
        } };

        // clang-format on

        constexpr GPIOA_Type* GpioTiva(Port port)
        {
            return portAndRcgc[static_cast<uint8_t>(port)].address;
        }

        constexpr uint32_t Rcgc(Port port)
        {
            return portAndRcgc[static_cast<uint8_t>(port)].rcgc;
        }

        constexpr uint32_t ToPin(uint8_t index)
        {
            return 1 << index;
        }

        uint8_t ToPctl(const infra::MemoryRange<const Gpio::PinPosition>& pins, Port port, uint8_t index)
        {
            for (auto& pin : pins)
                if (pin.pin == index && pin.port == port)
                    return pin.portControl;

            std::abort();
        }

        template<class T>
        constexpr bool GetBit(T& reg, uint32_t position)
        {
            return reg & (1 << position);
        }
    }

    DummyPin dummyPin;

    GpioPin::GpioPin(Port port, uint8_t index, Drive drive, Current current)
        : port(port)
        , index(index)
        , drive(drive)
        , current(current)
    {
        SYSCTL->RCGCGPIO = SYSCTL->RCGCGPIO | Rcgc(port);

        while (!(SYSCTL->PRGPIO & Rcgc(port)))
        {
        }

        if (family::IsLockProtected(GpioTiva(port), index))
        {
            GpioTiva(port)->LOCK = 0x4C4F434B;
            infra::ReplaceBit(GpioTiva(port)->CR, true, index);
            GpioTiva(port)->LOCK = 0;
        }
    }

    bool GpioPin::Get() const
    {
        return infra::IsBitSet(GpioTiva(port)->DATA, index);
    }

    void GpioPin::Set(bool value)
    {
        reinterpret_cast<volatile uint32_t*>(GpioTiva(port))[1u << index] = value ? (1u << index) : 0u;
    }

    bool GpioPin::GetOutputLatch() const
    {
        return infra::IsBitSet(GpioTiva(port)->DATA, index);
    }

    void GpioPin::SetAsInput()
    {
        infra::ReplaceBit(GpioTiva(port)->DIR, false, index);
    }

    bool GpioPin::IsInput() const
    {
        return GetBit(GpioTiva(port)->DIR, index) == 0;
    }

    void GpioPin::Config(PinConfigType config)
    {
        Gpio::Instance().ReservePin(port, index);

        infra::ReplaceBit(GpioTiva(port)->DIR, modeTiva[static_cast<uint8_t>(config)].dir, index);
        infra::ReplaceBit(GpioTiva(port)->AFSEL, false, index);
        infra::ReplaceBit(GpioTiva(port)->DR2R, currentDriveTiva[static_cast<uint8_t>(current)]._2mA, index);
        infra::ReplaceBit(GpioTiva(port)->DR4R, currentDriveTiva[static_cast<uint8_t>(current)]._4mA, index);
        infra::ReplaceBit(GpioTiva(port)->DR8R, currentDriveTiva[static_cast<uint8_t>(current)]._8mA, index);

        infra::ReplaceBit(GpioTiva(port)->DEN, modeTiva[static_cast<uint8_t>(config)].den, index);
        infra::ReplaceBit(GpioTiva(port)->AMSEL, modeTiva[static_cast<uint8_t>(config)].amsel, index);

        infra::ReplaceBit(GpioTiva(port)->PUR, pushPullTiva[static_cast<uint8_t>(drive)].pur, index);
        infra::ReplaceBit(GpioTiva(port)->PDR, pushPullTiva[static_cast<uint8_t>(drive)].pdr, index);
        infra::ReplaceBit(GpioTiva(port)->ODR, pushPullTiva[static_cast<uint8_t>(drive)].odr, index);

        GpioTiva(port)->PCTL = (GpioTiva(port)->PCTL & pinTiva[index].mask) | 0 << pinTiva[index].bits;
    }

    void GpioPin::Config(PinConfigType config, bool startOutputState)
    {
        Config(config);

        if (config != PinConfigType::input)
            Set(startOutputState);
    }

    void GpioPin::ResetConfig()
    {
        infra::ReplaceBit(GpioTiva(port)->DIR, false, index);
        infra::ReplaceBit(GpioTiva(port)->DEN, false, index);
        infra::ReplaceBit(GpioTiva(port)->AMSEL, false, index);

        infra::ReplaceBit(GpioTiva(port)->PUR, false, index);
        infra::ReplaceBit(GpioTiva(port)->PDR, false, index);
        infra::ReplaceBit(GpioTiva(port)->ODR, false, index);

        infra::ReplaceBit(GpioTiva(port)->AFSEL, false, index);
        GpioTiva(port)->PCTL = (GpioTiva(port)->PCTL & pinTiva[index].mask) | 0 << pinTiva[index].bits;

        infra::ReplaceBit(GpioTiva(port)->DR2R, true, index);
        infra::ReplaceBit(GpioTiva(port)->DR4R, false, index);
        infra::ReplaceBit(GpioTiva(port)->DR8R, false, index);

        Gpio::Instance().ClearPinReservation(port, index);
    }

    void GpioPin::EnableInterrupt(const infra::Function<void()>& action, InterruptTrigger trigger, InterruptType type)
    {
        Gpio::Instance().EnableInterrupt(port, index, action, trigger, type);
    }

    void GpioPin::DisableInterrupt()
    {
        Gpio::Instance().DisableInterrupt(port, index);
    }

    void GpioPin::ConfigAnalog()
    {
        Gpio::Instance().ReservePin(port, index);

        infra::ReplaceBit(GpioTiva(port)->DIR, false, index);
        infra::ReplaceBit(GpioTiva(port)->DEN, false, index);
        infra::ReplaceBit(GpioTiva(port)->AFSEL, true, index);
        infra::ReplaceBit(GpioTiva(port)->AMSEL, true, index);
    }

    void GpioPin::ConfigPeripheral(PinConfigPeripheral pinConfigType)
    {
        const auto& peripheralPinConfig = Gpio::Instance().GetPeripheralPinConfig(port, index, pinConfigType);

        Gpio::Instance().ReservePin(port, index);

        const auto& portControl = peripheralPinConfig.second;
        const auto& drive = peripheralPinConfig.second.drive;
        const auto& current = peripheralPinConfig.second.current;

        infra::ReplaceBit(GpioTiva(port)->DIR, modeTiva[static_cast<uint8_t>(portControl.config)].dir, index);
        infra::ReplaceBit(GpioTiva(port)->DEN, portControl.isDigital, index);
        infra::ReplaceBit(GpioTiva(port)->AMSEL, !portControl.isDigital, index);

        GpioTiva(port)->PCTL = (GpioTiva(port)->PCTL & pinTiva[index].mask) | ToPctl(portControl.pinPositions, port, index) << pinTiva[index].bits;

        infra::ReplaceBit(GpioTiva(port)->AFSEL, true, index);
        infra::ReplaceBit(GpioTiva(port)->DR2R, currentDriveTiva[static_cast<uint8_t>(current)]._2mA, index);
        infra::ReplaceBit(GpioTiva(port)->DR4R, currentDriveTiva[static_cast<uint8_t>(current)]._4mA, index);
        infra::ReplaceBit(GpioTiva(port)->DR8R, currentDriveTiva[static_cast<uint8_t>(current)]._8mA, index);

        infra::ReplaceBit(GpioTiva(port)->PUR, pushPullTiva[static_cast<uint8_t>(drive)].pur, index);
        infra::ReplaceBit(GpioTiva(port)->PDR, pushPullTiva[static_cast<uint8_t>(drive)].pdr, index);
        infra::ReplaceBit(GpioTiva(port)->ODR, pushPullTiva[static_cast<uint8_t>(drive)].odr, index);
    }

    uint32_t GpioPin::AdcChannel() const
    {
        return Gpio::Instance().AdcChannel(port, index);
    }

    DummyPin::DummyPin()
        : GpioPin(Port::A, 0)
    {}

    bool DummyPin::Get() const
    {
        return false;
    }

    void DummyPin::Set(bool value)
    {}

    bool DummyPin::GetOutputLatch() const
    {
        return false;
    }

    void DummyPin::SetAsInput()
    {}

    bool DummyPin::IsInput() const
    {
        return false;
    }

    void DummyPin::Config(PinConfigType config)
    {}

    void DummyPin::Config(PinConfigType config, bool startOutputState)
    {}

    void DummyPin::ResetConfig()
    {}

    void DummyPin::EnableInterrupt(const infra::Function<void()>& action, InterruptTrigger trigger, InterruptType type)
    {}

    void DummyPin::DisableInterrupt()
    {}

    void DummyPin::ConfigAnalog()
    {}

    void DummyPin::ConfigPeripheral(PinConfigPeripheral pinConfigType)
    {}

    PeripheralPin::PeripheralPin(GpioPin& pin, PinConfigPeripheral pinConfigType)
        : pin(pin)
    {
        pin.ConfigPeripheral(pinConfigType);
    }

    PeripheralPin::~PeripheralPin()
    {
        pin.ResetConfig();
    }

    AnalogPin::AnalogPin(GpioPin& pin)
        : pin(pin)
    {
        pin.ConfigAnalog();
    }

    AnalogPin::~AnalogPin()
    {
        pin.ResetConfig();
    }

    uint32_t AnalogPin::AdcChannel() const
    {
        return pin.AdcChannel();
    }

    MultiGpioPin::MultiGpioPin(infra::MemoryRange<const std::pair<Port, uint8_t>> table, Drive drive, Current current)
        : table(table)
        , drive(drive)
        , current(current)
    {
        for (const auto& portAndIndex : table)
        {
            SYSCTL->RCGCGPIO = SYSCTL->RCGCGPIO | Rcgc(portAndIndex.first); // NOLINT

            while (!(SYSCTL->PRGPIO & Rcgc(portAndIndex.first))) // NOLINT
            {
            }

            if (family::IsLockProtected(GpioTiva(portAndIndex.first), portAndIndex.second)) // NOLINT
            {
                GpioTiva(portAndIndex.first)->LOCK = 0x4C4F434B;
                infra::ReplaceBit(GpioTiva(portAndIndex.first)->CR, true, portAndIndex.second);
                GpioTiva(portAndIndex.first)->LOCK = 0;
            }
        }
    }

    void MultiGpioPin::ResetConfig()
    {
        for (const auto& portAndIndex : table)
        {
            infra::ReplaceBit(GpioTiva(portAndIndex.first)->DIR, false, portAndIndex.second);
            infra::ReplaceBit(GpioTiva(portAndIndex.first)->DEN, false, portAndIndex.second);
            infra::ReplaceBit(GpioTiva(portAndIndex.first)->AMSEL, false, portAndIndex.second);

            infra::ReplaceBit(GpioTiva(portAndIndex.first)->PUR, false, portAndIndex.second);
            infra::ReplaceBit(GpioTiva(portAndIndex.first)->PDR, false, portAndIndex.second);
            infra::ReplaceBit(GpioTiva(portAndIndex.first)->ODR, false, portAndIndex.second);

            infra::ReplaceBit(GpioTiva(portAndIndex.first)->AFSEL, false, portAndIndex.second);
            GpioTiva(portAndIndex.first)->PCTL = (GpioTiva(portAndIndex.first)->PCTL & pinTiva[portAndIndex.second].mask) | 0 << pinTiva[portAndIndex.second].bits;

            infra::ReplaceBit(GpioTiva(portAndIndex.first)->DR2R, true, portAndIndex.second);
            infra::ReplaceBit(GpioTiva(portAndIndex.first)->DR4R, false, portAndIndex.second);
            infra::ReplaceBit(GpioTiva(portAndIndex.first)->DR8R, false, portAndIndex.second);

            Gpio::Instance().ClearPinReservation(portAndIndex.first, portAndIndex.second);
        }
    }

    void MultiGpioPin::ConfigAnalog()
    {
        for (const auto& portAndIndex : table)
        {
            Gpio::Instance().ReservePin(portAndIndex.first, portAndIndex.second);

            infra::ReplaceBit(GpioTiva(portAndIndex.first)->DIR, false, portAndIndex.second);
            infra::ReplaceBit(GpioTiva(portAndIndex.first)->DEN, false, portAndIndex.second);
            infra::ReplaceBit(GpioTiva(portAndIndex.first)->AFSEL, true, portAndIndex.second);
            infra::ReplaceBit(GpioTiva(portAndIndex.first)->AMSEL, true, portAndIndex.second);
        }
    }

    void MultiGpioPin::ConfigPeripheral(PinConfigPeripheral pinConfigType)
    {
        for (const auto& portAndIndex : table)
        {
            const auto& peripheralPinConfig = Gpio::Instance().GetPeripheralPinConfig(portAndIndex.first, portAndIndex.second, pinConfigType);

            Gpio::Instance().ReservePin(portAndIndex.first, portAndIndex.second);

            const auto& portControl = peripheralPinConfig.second;

            infra::ReplaceBit(GpioTiva(portAndIndex.first)->DIR, modeTiva[static_cast<uint8_t>(portControl.config)].dir, portAndIndex.second);
            infra::ReplaceBit(GpioTiva(portAndIndex.first)->DEN, portControl.isDigital, portAndIndex.second);
            infra::ReplaceBit(GpioTiva(portAndIndex.first)->AMSEL, !portControl.isDigital, portAndIndex.second);

            GpioTiva(portAndIndex.first)->PCTL = (GpioTiva(portAndIndex.first)->PCTL & pinTiva[portAndIndex.second].mask) | 0 << pinTiva[portAndIndex.second].bits;

            infra::ReplaceBit(GpioTiva(portAndIndex.first)->PUR, pushPullTiva[static_cast<uint8_t>(drive)].pur, portAndIndex.second);
            infra::ReplaceBit(GpioTiva(portAndIndex.first)->PDR, pushPullTiva[static_cast<uint8_t>(drive)].pdr, portAndIndex.second);
            infra::ReplaceBit(GpioTiva(portAndIndex.first)->ODR, pushPullTiva[static_cast<uint8_t>(drive)].odr, portAndIndex.second);

            infra::ReplaceBit(GpioTiva(portAndIndex.first)->AFSEL, true, portAndIndex.second);
            GpioTiva(portAndIndex.first)->PCTL = (GpioTiva(portAndIndex.first)->PCTL & pinTiva[portAndIndex.second].mask) | ToPctl(portControl.pinPositions, portAndIndex.first, portAndIndex.second) << pinTiva[portAndIndex.second].bits;

            infra::ReplaceBit(GpioTiva(portAndIndex.first)->DR2R, currentDriveTiva[static_cast<uint8_t>(current)]._2mA, portAndIndex.second);
            infra::ReplaceBit(GpioTiva(portAndIndex.first)->DR4R, currentDriveTiva[static_cast<uint8_t>(current)]._4mA, portAndIndex.second);
            infra::ReplaceBit(GpioTiva(portAndIndex.first)->DR8R, currentDriveTiva[static_cast<uint8_t>(current)]._8mA, portAndIndex.second);
        }
    }

    MultiPeripheralPin::MultiPeripheralPin(MultiGpioPin& pins, PinConfigPeripheral pinConfigType)
        : pins(pins)
    {
        pins.ConfigPeripheral(pinConfigType);
    }

    MultiPeripheralPin::~MultiPeripheralPin()
    {
        pins.ResetConfig();
    }

    // clang-format off
    Gpio::Gpio(infra::MemoryRange<const infra::MemoryRange<const Gpio::PinoutTable>> pinoutTable, infra::MemoryRange<const Gpio::AnalogPinPosition> analogTable)
        : pinoutTable(pinoutTable)
        , analogTable(analogTable)
        , interruptTypes{}
        , assignedPins()
    {
        for (std::size_t i = 0; i < portAndRcgc.size(); ++i)
        {
            if (portAndRcgc[i].address == nullptr || portAndRcgc[i].irq < 0)
                continue;
            portHandlers[i].emplace(portAndRcgc[i].irq, [this, i]() { ExtiInterruptPort(i); });
        }

        for (std::size_t i = 0; i < portAndRcgc.size(); ++i)
        {
            if (portAndRcgc[i].address == nullptr || !portAndRcgc[i].perPin)
                continue;
            const std::size_t pinBase = (i - static_cast<std::size_t>(Port::P)) * 8;
            for (std::size_t pin = 0; pin < 8; ++pin)
            {
                const std::size_t ph = pinBase + pin;
                if (perPinIrqs[ph] >= 0)
                    pinHandlers[ph].emplace(perPinIrqs[ph], [this, h = i * 8 + pin]() { ExtiInterruptSinglePin(h); });
            }
        }
    }

    // clang-format on

    std::pair<const Gpio::PinPosition&, const Gpio::PinoutTable&> Gpio::GetPeripheralPinConfig(Port port, uint8_t index, PinConfigPeripheral pinConfigType) const
    {
        for (infra::MemoryRange<const Gpio::PinoutTable> subTable : pinoutTable)
            for (const PinoutTable& table : subTable)
                if (table.pinConfigType == pinConfigType)
                    for (const PinPosition& position : table.pinPositions)
                        if (position.port == port && position.pin == index)
                            return std::pair<const Gpio::PinPosition&, const Gpio::PinoutTable&>(position, table);

        abort();
    }

    uint32_t Gpio::AdcChannel(Port port, uint8_t pin) const
    {
        for (const Gpio::AnalogPinPosition position : analogTable)
            if (position.type == Type::adc && position.port == port && position.pin == pin)
                return position.channel;

        abort();
    }

    void Gpio::EnableInterrupt(Port port, uint8_t index, const infra::Function<void()>& action, InterruptTrigger trigger, InterruptType type)
    {
        const std::size_t portIdx = static_cast<uint8_t>(port);
        really_assert(portAndRcgc[portIdx].address != nullptr);
        really_assert(portAndRcgc[portIdx].irq >= 0 || portAndRcgc[portIdx].perPin);

        infra::ReplaceBit(GpioTiva(port)->IM, false, index);

        infra::ReplaceBit(GpioTiva(port)->IBE, interruptTiva[static_cast<uint8_t>(trigger)].ibe, index);
        infra::ReplaceBit(GpioTiva(port)->IS, interruptTiva[static_cast<uint8_t>(trigger)].is, index);
        infra::ReplaceBit(GpioTiva(port)->IEV, interruptTiva[static_cast<uint8_t>(trigger)].iev, index);

        const std::size_t handlerIndex = portIdx * 8 + index;
        really_assert(handlerIndex < handlers.size());
        really_assert(!handlers[handlerIndex]);
        handlers[handlerIndex] = action;
        interruptTypes[handlerIndex] = type;

        infra::ReplaceBit(GpioTiva(port)->ICR, true, index);
        infra::ReplaceBit(GpioTiva(port)->IM, true, index);
    }

    void Gpio::DisableInterrupt(Port port, uint8_t index)
    {
        infra::ReplaceBit(GpioTiva(port)->IM, false, index);

        const std::size_t handlerIndex = static_cast<uint8_t>(port) * 8 + index;
        really_assert(handlerIndex < handlers.size());
        handlers[handlerIndex] = nullptr;
    }

    void Gpio::ExtiInterrupt(GPIOA_Type* gpio, std::size_t portIndex, std::size_t from, std::size_t to)
    {
        for (std::size_t line = from; line != to; ++line)
        {
            if (infra::IsBitSet(gpio->RIS, line))
            {
                infra::ReplaceBit(gpio->ICR, true, line);

                const std::size_t h = portIndex * 8 + line;
                if (handlers[h])
                {
                    if (interruptTypes[h] == InterruptType::immediate)
                        handlers[h]();
                    else
                        infra::EventDispatcher::Instance().Schedule(handlers[h]);
                }
            }
        }
    }

    void Gpio::ExtiInterruptPort(std::size_t portIndex)
    {
        ExtiInterrupt(portAndRcgc[portIndex].address, portIndex, 0, 8);
    }

    void Gpio::ExtiInterruptSinglePin(std::size_t handlerIndex)
    {
        const std::size_t portIndex = handlerIndex / 8;
        const std::size_t pin = handlerIndex % 8;
        ExtiInterrupt(portAndRcgc[portIndex].address, portIndex, pin, pin + 1);
    }

    void Gpio::ReservePin(Port port, uint8_t index)
    {
        really_assert(static_cast<uint8_t>(port) < assignedPins.size());
        really_assert(index < 8);
        really_assert((assignedPins[static_cast<uint8_t>(port)] & (1 << index)) == 0);
        assignedPins[static_cast<uint8_t>(port)] |= 1 << index;
    }

    void Gpio::ClearPinReservation(Port port, uint8_t index)
    {
        really_assert(static_cast<uint8_t>(port) < assignedPins.size());
        really_assert(index < 8);
        assignedPins[static_cast<uint8_t>(port)] &= ~(1 << index);
    }
}
