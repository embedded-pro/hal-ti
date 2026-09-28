#include "validation/firmware/EthernetCommands.hpp"
#include "hal_tiva/tiva/Ethernet.hpp"
#include "hal_tiva/tiva/UniqueDeviceId.hpp"
#include <optional>

namespace validation
{
    namespace
    {
        enum class Speed : uint8_t
        {
            automatic,
            _10,
            _100,
        };

        constexpr std::array<Choice<hal::tiva::Ethernet::PhySelection>, 1> phys{ {
            { "internal", hal::tiva::Ethernet::PhySelection::internal },
        } };

        constexpr std::array<Choice<Speed>, 3> speeds{ {
            { "auto", Speed::automatic },
            { "10", Speed::_10 },
            { "100", Speed::_100 },
        } };

        class EthernetMonitor
            : public hal::EthernetSmiObserver
            , public hal::EthernetMacObserver
        {
        public:
            explicit EthernetMonitor(hal::tiva::Ethernet& ethernet)
                : hal::EthernetSmiObserver(ethernet)
                , hal::EthernetMacObserver(ethernet)
            {}

            void LinkUp(hal::LinkSpeed speed) override
            {
                up = true;
                linkSpeed = speed;
            }

            void LinkDown() override
            {
                up = false;
            }

            infra::ByteRange RequestReceiveBuffer() override
            {
                auto& buffer = buffers[nextBuffer];
                nextBuffer = (nextBuffer + 1) % buffers.size();
                return infra::MakeRange(buffer);
            }

            void ReceivedFrame(uint32_t, uint32_t) override
            {
                ++received;
            }

            void ReceivedErrorFrame(uint32_t, uint32_t) override
            {}

            void SentFrame() override
            {
                ++sent;
            }

            bool up = false;
            hal::LinkSpeed linkSpeed = hal::LinkSpeed::halfDuplex10MHz;
            uint32_t received = 0;
            uint32_t sent = 0;

        private:
            std::array<std::array<uint8_t, 1536>, 4> buffers{};
            std::size_t nextBuffer = 0;
        };

        struct Instance
        {
            Instance(hal::tiva::Ethernet::PhySelection phy, hal::LinkSpeed speed, hal::MacAddress macAddress)
                : ethernet(hal::tiva::Ethernet::Leds{}, phy, speed, macAddress)
            {}

            hal::tiva::Ethernet ethernet;
            EthernetMonitor monitor{ ethernet };
        };

        std::optional<Instance>& Storage()
        {
            static std::optional<Instance> instance;
            return instance;
        }

        hal::MacAddress LocallyAdministeredAddress()
        {
            hal::MacAddress address{ { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 } };
            auto uid = hal::tiva::UniqueDeviceId();

            for (std::size_t i = 0; i != uid.size(); ++i)
                address[2 + i % 4] ^= uid[i];

            return address;
        }
    }

    EthernetCommands::EthernetCommands(Context& context)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , commands{ {
              Bind<EthernetCommands, &EthernetCommands::Open>("eth.open", "[phy=internal] [speed=auto|10|100]", *this, context.response),
              Bind<EthernetCommands, &EthernetCommands::LinkStatus>("eth.status", "", *this, context.response),
              Bind<EthernetCommands, &EthernetCommands::Close>("eth.close", "", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> EthernetCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    Status EthernetCommands::Open(const Arguments& arguments)
    {
        if (!arguments.Shape(0, 0, { "phy", "speed" }))
            return Status::usage;

        auto phy = hal::tiva::Ethernet::PhySelection::internal;
        auto speed = Speed::automatic;
        Status status = Status::done;
        arguments.Select("phy", phy, phys, status);
        arguments.Select("speed", speed, speeds, status);
        if (status != Status::done)
            return status;

        if (Storage())
            return Status::busy;

        Storage().emplace(phy, speed == Speed::_10 ? hal::LinkSpeed::fullDuplex10MHz : hal::LinkSpeed::fullDuplex100MHz, LocallyAdministeredAddress());
        context.response.Ok();
        return Status::done;
    }

    Status EthernetCommands::LinkStatus(const Arguments& arguments)
    {
        if (!arguments.Shape(0, 0, {}))
            return Status::usage;

        if (!Storage())
            return Status::notOpen;

        const auto& monitor = Storage()->monitor;
        const bool fast = monitor.linkSpeed == hal::LinkSpeed::fullDuplex100MHz || monitor.linkSpeed == hal::LinkSpeed::halfDuplex100MHz;
        const bool fullDuplex = monitor.linkSpeed == hal::LinkSpeed::fullDuplex100MHz || monitor.linkSpeed == hal::LinkSpeed::fullDuplex10MHz;

        context.response.Ok() << " link=" << (monitor.up ? "up" : "down") << " speed=" << (fast ? 100u : 10u) << " duplex=" << (fullDuplex ? "full" : "half")
                              << " rx=" << monitor.received << " tx=" << monitor.sent;
        return Status::done;
    }

    Status EthernetCommands::Close(const Arguments& arguments)
    {
        if (!arguments.Shape(0, 0, {}))
            return Status::usage;

        if (!Storage())
            return Status::notOpen;

        Storage() = std::nullopt;
        context.response.Ok();
        return Status::done;
    }
}
