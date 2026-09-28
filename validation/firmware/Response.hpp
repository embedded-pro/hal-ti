#ifndef VALIDATION_RESPONSE_HPP
#define VALIDATION_RESPONSE_HPP

#include "infra/stream/OutputStream.hpp"
#include "infra/util/BoundedString.hpp"
#include "infra/util/ByteRange.hpp"
#include "services/tracer/Tracer.hpp"
#include "validation/firmware/BoardTypes.hpp"
#include <cstdint>

namespace validation
{
    enum class Status : uint8_t
    {
        done,
        usage,
        pin,
        busy,
        notOpen,
        unsupported,
        range,
        timeout,
        failed,
    };

    class Response
    {
    public:
        class Line
        {
        public:
            Line(Response& response, const char* head, const char* subject = nullptr);
            Line(const Line& other) = delete;
            Line& operator=(const Line& other) = delete;
            ~Line();

            Line& operator<<(const char* text);
            Line& operator<<(infra::BoundedConstString text);
            Line& operator<<(uint32_t value);
            Line& operator<<(PinId pin);
            Line& Hex(infra::ConstByteRange data);

        private:
            infra::TextOutputStream stream;
        };

        explicit Response(services::Tracer& tracer);

        void BeginCommand();
        void EndCommand();

        Line Ok();
        Line Event(const char* peripheral);
        void Error(Status status);

    private:
        infra::TextOutputStream StartLine();

    private:
        services::Tracer& tracer;
        bool inCommand = false;
    };
}

#endif
