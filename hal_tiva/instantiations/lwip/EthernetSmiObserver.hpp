#pragma once

#include "hal/interfaces/Ethernet.hpp"
#include "lwip/lwip_cpp/LightweightIpOverEthernet.hpp"

namespace instantiations
{
    class EthernetSmiObserver
        : public hal::EthernetSmiObserver
    {
    public:
        explicit EthernetSmiObserver(hal::EthernetSmi& ethernetSmi, hal::EthernetMac& ethernetMac, services::LightweightIpOverEthernetFactory& lightweightIpOverEthernetFactory);
        ~EthernetSmiObserver();

        void LinkUp(hal::LinkSpeed linkSpeed) override;
        void LinkDown() override;

    private:
        services::LightweightIpOverEthernetFactory& lightweightIpOverEthernetFactory;
        hal::EthernetMac& ethernetMac;
    };
}
