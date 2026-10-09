#include "EthernetDemo.hpp"
#include "services/network/http/HttpErrors.hpp"

namespace demo
{
    namespace
    {
        constexpr uint16_t httpPort = 80;
        const infra::BoundedConstString rootBody = "<h1>EK-TM4C1294XL</h1><p>Served by hal-ti and EMIL.</p><p><a href=\"/led\">Toggle LED D2</a></p>";
    }

    EthernetDemo::EthernetDemo(services::LightweightIp& lightweightIp, infra::BoundedConstString hostName, hal::GpioPin& led, services::Tracer& tracer)
        : rootPage("", rootBody, "text/html")
        , ledPage(led, tracer)
        , httpServer(lightweightIp, httpPort)
        , llmnrResponder(lightweightIp, lightweightIp, lightweightIp, hostName)
    {
        httpServer.AddPage(rootPage);
        httpServer.AddPage(ledPage);

        tracer.Trace() << "IPv4 address " << lightweightIp.GetIPv4Address() << ", HTTP on port " << httpPort << ", LLMNR name " << hostName;
    }

    void EthernetDemo::Stop(const infra::Function<void()>& onDone)
    {
        onDone();
    }

    EthernetDemo::LedPage::LedPage(hal::GpioPin& led, services::Tracer& tracer)
        : led(led)
        , tracer(tracer)
        , ledOn(services::http_responses::ok, "LED D2 on")
        , ledOff(services::http_responses::ok, "LED D2 off")
    {}

    bool EthernetDemo::LedPage::ServesRequest(const infra::Tokenizer& pathTokens) const
    {
        return pathTokens.TokenAndRest(0) == infra::BoundedConstString("led");
    }

    void EthernetDemo::LedPage::RespondToRequest(services::HttpRequestParser& parser, services::HttpServerConnection& connection)
    {
        if (parser.Verb() != services::HttpVerb::get)
        {
            connection.SendResponse(services::HttpResponseMethodNotAllowed::Instance());
            return;
        }

        led.Set(!led.GetOutputLatch());
        tracer.Trace() << "LED D2 " << (led.GetOutputLatch() ? "on" : "off");
        connection.SendResponse(led.GetOutputLatch() ? ledOn : ledOff);
    }
}
