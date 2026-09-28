#include "validation/firmware/EthernetGroup.hpp"
#include "hal_tiva/tiva/Ethernet.hpp"
#include "hal_tiva/tiva/UniqueDeviceId.hpp"
#include "services/hil/commands/HilEthernetCommands.hpp"
#include <optional>

namespace validation
{
    namespace
    {
        using services::HilChoice;
        using services::HilStatus;

        enum class Speed : uint8_t
        {
            automatic,
            _10,
            _100,
        };

        constexpr std::array<const char*, 2> openKeys{ { "phy", "speed" } };

        constexpr std::array<HilChoice<hal::tiva::Ethernet::PhySelection>, 1> phys{ {
            { "internal", hal::tiva::Ethernet::PhySelection::internal },
        } };

        constexpr std::array<HilChoice<Speed>, 3> speeds{ {
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
            : public services::HilEthernetFactory
        {
        public:
            infra::MemoryRange<const char* const> OpenKeys() const override
            {
                return infra::MakeRange(openKeys);
            }

            HilStatus Prepare(const services::HilArguments& arguments) override
            {
                auto phy = hal::tiva::Ethernet::PhySelection::internal;
                auto speed = Speed::automatic;
                return Parse(arguments, phy, speed);
            }

            HilStatus Open(const services::HilArguments& arguments, services::HilEthernetHandle& handle) override
            {
                auto phy = hal::tiva::Ethernet::PhySelection::internal;
                auto speed = Speed::automatic;
                Parse(arguments, phy, speed);

                auto& opened = ethernet.emplace(hal::tiva::Ethernet::Leds{}, phy, speed == Speed::_10 ? hal::LinkSpeed::fullDuplex10MHz : hal::LinkSpeed::fullDuplex100MHz, LocallyAdministeredAddress());
                handle.smi = &opened;
                handle.mac = &opened;
                return HilStatus::done;
            }

            void Close(const infra::Function<void()>& onClosed) override
            {
                ethernet = std::nullopt;
                onClosed();
            }

        private:
            static HilStatus Parse(const services::HilArguments& arguments, hal::tiva::Ethernet::PhySelection& phy, Speed& speed)
            {
                HilStatus status = HilStatus::done;
                arguments.Select("phy", phy, phys, status);
                arguments.Select("speed", speed, speeds, status);
                return status;
            }

        private:
            std::optional<hal::tiva::Ethernet> ethernet;
        };
    }

    void CreateEthernetGroup(services::HilContext& context)
    {
        static TivaEthernetFactory factory;
        static services::HilEthernetCommands::WithReceiveBuffers<4> ethernet{ context, factory };
    }
}
