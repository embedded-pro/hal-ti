#ifndef DEMO_EK_TM4C1294XL_ETHERNET_DEMO_HPP
#define DEMO_EK_TM4C1294XL_ETHERNET_DEMO_HPP

#include "hal/interfaces/Gpio.hpp"
#include "infra/util/BoundedString.hpp"
#include "lwip/lwip_cpp/LightweightIp.hpp"
#include "services/network/dns/LlmnrResponder.hpp"
#include "services/network/http/HttpServer.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/Stoppable.hpp"

namespace demo
{
    class EthernetDemo
        : public services::Stoppable
    {
    public:
        EthernetDemo(services::LightweightIp& lightweightIp, infra::BoundedConstString hostName, hal::GpioPin& led, services::Tracer& tracer);

        void Stop(const infra::Function<void()>& onDone) override;

    private:
        class LedPage
            : public services::SimpleHttpPage
        {
        public:
            LedPage(hal::GpioPin& led, services::Tracer& tracer);

            bool ServesRequest(const infra::Tokenizer& pathTokens) const override;
            void RespondToRequest(services::HttpRequestParser& parser, services::HttpServerConnection& connection) override;

        private:
            hal::OutputPin led;
            services::Tracer& tracer;
            services::SimpleHttpResponse ledOn;
            services::SimpleHttpResponse ledOff;
        };

    private:
        services::HttpPageWithContent rootPage;
        LedPage ledPage;
        services::DefaultHttpServer::WithBuffer<1024> httpServer;
        services::LlmnrResponder llmnrResponder;
    };
}

#endif
