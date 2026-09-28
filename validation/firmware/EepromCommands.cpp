#include "validation/firmware/EepromCommands.hpp"
#include "infra/event/EventDispatcher.hpp"

namespace validation
{
    namespace
    {
        constexpr infra::Duration operationTimeout = std::chrono::seconds(5);
    }

    EepromCommands::EepromCommands(Context& context)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , commands{ {
              Bind<EepromCommands, &EepromCommands::Write>("eeprom.write", "<address> <hex>", *this, context.response),
              Bind<EepromCommands, &EepromCommands::Read>("eeprom.read", "<address> <len>", *this, context.response),
              Bind<EepromCommands, &EepromCommands::Erase>("eeprom.erase", "", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> EepromCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    Status EepromCommands::Write(const Arguments& arguments)
    {
        if (!arguments.Shape(2, 2, {}))
            return Status::usage;

        uint32_t address = 0;
        std::size_t size = 0;
        Status status = Status::done;
        arguments.NumberAt(0, address, 0, Instance().Size(), status);
        if (status == Status::done)
            status = ParseHex(arguments.Positional(1), buffer, size);
        if (status != Status::done)
            return status;

        if (size == 0)
            return Status::usage;

        if (size > Instance().Size() - address)
            return Status::range;

        if (operating)
            return Status::busy;

        Start();
        Instance().WriteBuffer(infra::ConstByteRange(buffer.data(), buffer.data() + size), address, [this]()
            {
                infra::EventDispatcher::Instance().Schedule([this]()
                    {
                        Done();
                    });
            });

        return Status::done;
    }

    Status EepromCommands::Read(const Arguments& arguments)
    {
        if (!arguments.Shape(2, 2, {}))
            return Status::usage;

        uint32_t address = 0;
        uint32_t length = 0;
        Status status = Status::done;
        arguments.NumberAt(0, address, 0, Instance().Size(), status);
        arguments.NumberAt(1, length, 1, capacity, status);
        if (status != Status::done)
            return status;

        if (length > Instance().Size() - address)
            return Status::range;

        if (operating)
            return Status::busy;

        auto data = infra::ByteRange(buffer.data(), buffer.data() + length);
        Instance().ReadBuffer(data, address, infra::emptyFunction);
        (context.response.Ok() << " data=").Hex(data);
        return Status::done;
    }

    Status EepromCommands::Erase(const Arguments& arguments)
    {
        if (!arguments.Shape(0, 0, {}))
            return Status::usage;

        if (operating)
            return Status::busy;

        Start();
        Instance().Erase([this]()
            {
                Done();
            });

        return Status::done;
    }

    hal::tiva::Eeprom& EepromCommands::Instance()
    {
        if (!eeprom)
            eeprom.emplace();

        return *eeprom;
    }

    void EepromCommands::Start()
    {
        operating = true;
        awaiting = true;
        timer.Start(operationTimeout, [this]()
            {
                Timeout();
            });
    }

    void EepromCommands::Done()
    {
        operating = false;

        if (awaiting)
        {
            awaiting = false;
            timer.Cancel();
            context.response.Ok();
        }
    }

    void EepromCommands::Timeout()
    {
        if (!awaiting)
            return;

        awaiting = false;
        context.response.Error(Status::timeout);
    }
}
