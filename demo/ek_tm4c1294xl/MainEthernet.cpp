#include "BoardIdentity.hpp"
#include "EthernetDemo.hpp"
#include "hal_tiva/instantiations/EventInfrastructure.hpp"
#include "hal_tiva/instantiations/LaunchPadBsp.hpp"
#include "hal_tiva/instantiations/lwip/Ethernet.hpp"
#include "infra/util/BoundedString.hpp"
#include "infra/util/ProxyCreator.hpp"
#include "services/peripheral/DebugLed.hpp"

int main()
{
    static instantiations::LaunchPad launchPad;
    static instantiations::EventInfrastructure eventInfrastructure;
    static instantiations::LaunchPadTerminalAndTracer terminal;

    static services::DebugLed heartbeat{ launchPad.DebugLed(), std::chrono::milliseconds(100), std::chrono::milliseconds(1400) };

    static hal::tiva::GpioPin linkLed{ hal::tiva::Port::F, 0 };
    static hal::tiva::GpioPin activityLed{ hal::tiva::Port::F, 4 };

    static demo::UniqueIdRandomGenerator randomDataGenerator;
    static infra::BoundedString::WithStorage<32> hostName{ "ek-tm4c1294xl" };
    static instantiations::Ethernet<1, 1, 3> ethernet{ { linkLed, activityLed }, demo::BoardMacAddress(), hostName, randomDataGenerator };

    static infra::Creator<services::Stoppable, demo::EthernetDemo, void(services::LightweightIp & lightweightIp)> ethernetDemo{ [](std::optional<demo::EthernetDemo>& value, services::LightweightIp& lightweightIp)
        {
            value.emplace(lightweightIp, hostName, launchPad.SecondLed(), terminal.tracer);
        } };
    ethernet.connectedCreator = &ethernetDemo;

    terminal.tracer.Trace() << "EK-TM4C1294XL Ethernet demo, waiting for a DHCP lease";
    terminal.tracer.Trace() << "MAC address " << infra::AsMacAddressHelper(demo::BoardMacAddress());

    eventInfrastructure.Run();
    __builtin_unreachable();
}
