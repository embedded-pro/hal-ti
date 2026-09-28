#include "validation/firmware/EthernetGroup.hpp"
#include "hal_tiva/tiva/Ethernet.hpp"
#include "hal_tiva/tiva/UniqueDeviceId.hpp"
#include "services/hil/commands/EthernetCommands.hpp"
#include <optional>

namespace validation
{
    namespace
    {
        using services::hil::Choice;
        using services::hil::Status;

        enum class Speed : uint8_t
        {
            automatic,
            _10,
            _100,
        };

        constexpr std::array<const char*, 2> openKeys{ { "phy", "speed" } };

        constexpr std::array<Choice<hal::tiva::Ethernet::PhySelection>, 1> phys{ {
            { "internal", hal::tiva::Ethernet::PhySelection::internal },
        } };

        constexpr std::array<Choice<Speed>, 3> speeds{ {
            { "auto", Speed::automatic },
            { "10", Speed::_10 },
            { "100", Speed::_100 },
        } };

        hal::MacAddress LocallyAdministeredAddress()
        {
            hal::MacAddress address{ { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 } };
            auto uid = hal::tiva::UniqueDeviceId();

            for (std::size_t i = 0; i != uid.size(); ++i)
                address[2 + i % 4] ^= uid[i];

            return address;
        }

        class TivaEthernetFactory
            : public services::hil::EthernetFactory
        {
        public:
            infra::MemoryRange<const char* const> OpenKeys() const override
            {
                return infra::MakeRange(openKeys);
            }

            Status Prepare(const services::hil::Arguments& arguments) override
            {
                auto phy = hal::tiva::Ethernet::PhySelection::internal;
                auto speed = Speed::automatic;
                return Parse(arguments, phy, speed);
            }

            Status Open(const services::hil::Arguments& arguments, services::hil::EthernetHandle& handle) override
            {
                auto phy = hal::tiva::Ethernet::PhySelection::internal;
                auto speed = Speed::automatic;
                Parse(arguments, phy, speed);

                auto& opened = ethernet.emplace(hal::tiva::Ethernet::Leds{}, phy, speed == Speed::_10 ? hal::LinkSpeed::fullDuplex10MHz : hal::LinkSpeed::fullDuplex100MHz, LocallyAdministeredAddress());
                handle.smi = &opened;
                handle.mac = &opened;
                return Status::done;
            }

            void Close(const infra::Function<void()>& onClosed) override
            {
                ethernet = std::nullopt;
                onClosed();
            }

        private:
            static Status Parse(const services::hil::Arguments& arguments, hal::tiva::Ethernet::PhySelection& phy, Speed& speed)
            {
                Status status = Status::done;
                arguments.Select("phy", phy, phys, status);
                arguments.Select("speed", speed, speeds, status);
                return status;
            }

        private:
            std::optional<hal::tiva::Ethernet> ethernet;
        };
    }

    void CreateEthernetGroup(services::hil::Context& context)
    {
        static TivaEthernetFactory factory;
        static services::hil::EthernetCommands::WithReceiveBuffers<4> ethernet{ context, factory };
    }
}
