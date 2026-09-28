#include "validation/firmware/SpiCommands.hpp"
#include "BoardProfile.hpp"
#include <algorithm>

extern "C" uint32_t SystemCoreClock;

namespace validation
{
    namespace
    {
        constexpr uint32_t maximumPrescaler = 254;
        constexpr uint32_t maximumSerialClockRate = 256;
        constexpr infra::Duration transferTimeout = std::chrono::milliseconds(1000);
    }

    SpiCommands::SpiCommands(Context& context)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , commands{ {
              Bind<SpiCommands, &SpiCommands::Open>("spi.open", "<index> clk= mosi= miso= [cs=] [baud=] [mode=] [sync=]", *this, context.response),
              Bind<SpiCommands, &SpiCommands::Transfer>("spi.xfer", "<index> <txHex|-> [rx=] [continue=]", *this, context.response),
              Bind<SpiCommands, &SpiCommands::Close>("spi.close", "<index>", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> SpiCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    Status SpiCommands::Open(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, { "clk", "mosi", "miso", "cs", "baud", "mode", "sync" }))
            return Status::usage;

        uint32_t requested = 0;
        std::optional<PinId> clock;
        std::optional<PinId> mosi;
        std::optional<PinId> miso;
        std::optional<PinId> chipSelect;
        uint32_t baud = 100000;
        uint32_t mode = 0;
        bool synchronous = false;

        Status status = Status::done;
        arguments.NumberAt(0, requested, 0, board::ssis - 1, status);
        arguments.Pin("clk", clock, status);
        arguments.Pin("mosi", mosi, status);
        arguments.Pin("miso", miso, status);
        arguments.Pin("cs", chipSelect, status);
        arguments.Number("baud", baud, SystemCoreClock / (maximumPrescaler * maximumSerialClockRate) + 1, SystemCoreClock / 2, status);
        arguments.Number("mode", mode, 0, 3, status);
        arguments.Flag("sync", synchronous, status);
        if (status != Status::done)
            return status;

        if (!clock || !mosi || !miso)
            return Status::usage;

        if (synchronous && chipSelect)
            return Status::unsupported;

        if (index)
            return Status::busy;

        const auto ssi = static_cast<uint8_t>(requested);
        hal::tiva::GpioPin* clockPin = nullptr;
        hal::tiva::GpioPin* mosiPin = nullptr;
        hal::tiva::GpioPin* misoPin = nullptr;
        hal::tiva::GpioPin* chipSelectPin = nullptr;
        status = context.pins.ClaimFunction(clock, owner::spi, hal::tiva::PinConfigPeripheral::spiClock, ssi, clockPin);
        if (status == Status::done)
            status = context.pins.ClaimFunction(mosi, owner::spi, hal::tiva::PinConfigPeripheral::spiMosi, ssi, mosiPin);
        if (status == Status::done)
            status = context.pins.ClaimFunction(miso, owner::spi, hal::tiva::PinConfigPeripheral::spiMiso, ssi, misoPin);
        if (status == Status::done)
            status = context.pins.ClaimFunction(chipSelect, owner::spi, hal::tiva::PinConfigPeripheral::spiSlaveSelect, ssi, chipSelectPin);

        if (status != Status::done)
        {
            context.pins.Release(owner::spi);
            return status;
        }

        const bool polarityLow = (mode & 2) == 0;
        const bool phaseFirst = (mode & 1) == 0;

        if (synchronous)
            driver.emplace<hal::tiva::SynchronousSpiMaster>(ssi, *clockPin, *misoPin, *mosiPin, hal::tiva::SynchronousSpiMaster::Config(polarityLow, phaseFirst, baud));
        else
        {
            hal::tiva::SpiMaster::Config config;
            config.polarityLow = polarityLow;
            config.phase1st = phaseFirst;
            config.baudRate = baud;
            driver.emplace<hal::tiva::SpiMaster>(ssi, *clockPin, *misoPin, *mosiPin, config, PinOrDummy(chipSelectPin));
        }

        index = ssi;
        context.response.Ok();
        return Status::done;
    }

    Status SpiCommands::Transfer(const Arguments& arguments)
    {
        if (!arguments.Shape(2, 2, { "rx", "continue" }))
            return Status::usage;

        std::size_t transmitSize = 0;
        uint32_t receiveSize = 0;
        bool continueSession = false;
        Status status = Find(arguments);
        if (status == Status::done)
            status = ParseHex(arguments.Positional(1), transmitBuffer, transmitSize);
        receiveSize = static_cast<uint32_t>(transmitSize);
        arguments.Number("rx", receiveSize, 0, capacity, status);
        arguments.Flag("continue", continueSession, status);
        if (status != Status::done)
            return status;

        if (transferring)
            return Status::busy;

        const auto length = std::max<std::size_t>(transmitSize, receiveSize);
        if (length == 0)
            return Status::usage;

        std::fill(transmitBuffer.begin() + transmitSize, transmitBuffer.begin() + length, 0);
        std::fill(receiveBuffer.begin(), receiveBuffer.end(), 0);

        auto send = transmitSize != 0 ? infra::ConstByteRange(transmitBuffer.data(), transmitBuffer.data() + length) : infra::ConstByteRange();
        auto receive = receiveSize != 0 ? infra::ByteRange(receiveBuffer.data(), receiveBuffer.data() + length) : infra::ByteRange();
        reportSize = receiveSize;

        if (auto synchronous = std::get_if<hal::tiva::SynchronousSpiMaster>(&driver))
        {
            synchronous->SendAndReceive(send, receive, continueSession ? hal::SynchronousSpi::continueSession : hal::SynchronousSpi::stop);
            Report();
            return Status::done;
        }

        transferring = true;
        awaiting = true;
        const auto current = ++generation;
        timer.Start(transferTimeout, [this]()
            {
                Timeout();
            });

        std::get<hal::tiva::SpiMaster>(driver).SendAndReceive(send, receive, continueSession ? hal::SpiAction::continueSession : hal::SpiAction::stop, [this, current]()
            {
                Done(current);
            });

        return Status::done;
    }

    Status SpiCommands::Close(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return Status::usage;

        Status status = Find(arguments);
        if (status != Status::done)
            return status;

        driver.emplace<std::monostate>();
        context.pins.Release(owner::spi);
        index = std::nullopt;
        ++generation;
        transferring = false;
        awaiting = false;
        timer.Cancel();
        context.response.Ok();
        return Status::done;
    }

    Status SpiCommands::Find(const Arguments& arguments)
    {
        uint32_t requested = 0;
        Status status = Status::done;
        arguments.NumberAt(0, requested, 0, board::ssis - 1, status);
        if (status != Status::done)
            return status;

        if (index != requested)
            return Status::notOpen;

        return Status::done;
    }

    void SpiCommands::Done(uint32_t current)
    {
        if (current != generation)
            return;

        transferring = false;

        if (awaiting)
        {
            awaiting = false;
            timer.Cancel();
            Report();
        }
    }

    void SpiCommands::Timeout()
    {
        if (!awaiting)
            return;

        awaiting = false;
        context.response.Error(Status::timeout);
    }

    void SpiCommands::Report()
    {
        (context.response.Ok() << " rx=").Hex(infra::ConstByteRange(receiveBuffer.data(), receiveBuffer.data() + reportSize));
    }
}
