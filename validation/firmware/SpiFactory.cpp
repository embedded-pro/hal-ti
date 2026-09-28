#include "validation/firmware/SpiFactory.hpp"
#include "BoardProfile.hpp"
#include "validation/firmware/TivaPinFactory.hpp"

extern "C" uint32_t SystemCoreClock;

namespace validation
{
    namespace
    {
        using services::hil::Status;

        constexpr uint32_t maximumPrescaler = 254;
        constexpr uint32_t maximumSerialClockRate = 256;

        constexpr std::array<const char*, 7> openKeys{ { "clk", "mosi", "miso", "cs", "baud", "mode", "sync" } };
    }

    TivaSpiFactory::TivaSpiFactory(const services::hil::PinNaming& naming)
        : naming(naming)
    {}

    uint8_t TivaSpiFactory::Instances() const
    {
        return board::ssis;
    }

    infra::MemoryRange<const char* const> TivaSpiFactory::OpenKeys() const
    {
        return infra::MakeRange(openKeys);
    }

    Status TivaSpiFactory::Prepare(uint8_t, const services::hil::Arguments& arguments)
    {
        Request request;
        return Parse(arguments, request);
    }

    Status TivaSpiFactory::Open(uint8_t index, const services::hil::Arguments& arguments, services::hil::PinOwner& pins, services::hil::SpiHandle& handle)
    {
        Request request;
        Parse(arguments, request);

        hal::GpioPin* clock = nullptr;
        hal::GpioPin* mosi = nullptr;
        hal::GpioPin* miso = nullptr;
        hal::GpioPin* chipSelect = nullptr;
        Status status = pins.ClaimFunction(request.clock, Function(hal::tiva::PinConfigPeripheral::spiClock), index, clock);
        if (status == Status::done)
            status = pins.ClaimFunction(request.mosi, Function(hal::tiva::PinConfigPeripheral::spiMosi), index, mosi);
        if (status == Status::done)
            status = pins.ClaimFunction(request.miso, Function(hal::tiva::PinConfigPeripheral::spiMiso), index, miso);
        if (status == Status::done)
            status = pins.ClaimFunction(request.chipSelect, Function(hal::tiva::PinConfigPeripheral::spiSlaveSelect), index, chipSelect);
        if (status != Status::done)
            return status;

        const bool polarityLow = (request.mode & 2) == 0;
        const bool phaseFirst = (request.mode & 1) == 0;

        if (request.synchronous)
            handle.synchronous = &driver.emplace<hal::tiva::SynchronousSpiMaster>(index, PinOrDummy(clock), PinOrDummy(miso), PinOrDummy(mosi), hal::tiva::SynchronousSpiMaster::Config(polarityLow, phaseFirst, request.baud));
        else
        {
            hal::tiva::SpiMaster::Config config;
            config.polarityLow = polarityLow;
            config.phase1st = phaseFirst;
            config.baudRate = request.baud;
            handle.spi = &driver.emplace<hal::tiva::SpiMaster>(index, PinOrDummy(clock), PinOrDummy(miso), PinOrDummy(mosi), config, PinOrDummy(chipSelect));
        }

        return Status::done;
    }

    void TivaSpiFactory::Close(uint8_t, const infra::Function<void()>& onClosed)
    {
        driver.emplace<std::monostate>();
        onClosed();
    }

    Status TivaSpiFactory::Parse(const services::hil::Arguments& arguments, Request& request) const
    {
        Status status = Status::done;
        arguments.Pin("clk", naming, request.clock, status);
        arguments.Pin("mosi", naming, request.mosi, status);
        arguments.Pin("miso", naming, request.miso, status);
        arguments.Pin("cs", naming, request.chipSelect, status);
        arguments.Number("baud", request.baud, SystemCoreClock / (maximumPrescaler * maximumSerialClockRate) + 1, SystemCoreClock / 2, status);
        arguments.Number("mode", request.mode, 0, 3, status);
        arguments.Flag("sync", request.synchronous, status);
        if (status != Status::done)
            return status;

        if (!request.clock || !request.mosi || !request.miso)
            return Status::usage;

        if (request.synchronous && request.chipSelect)
            return Status::unsupported;

        return Status::done;
    }
}
