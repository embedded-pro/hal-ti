#include "validation/firmware/Response.hpp"
#include <array>

namespace validation
{
    namespace
    {
        constexpr std::array<const char*, 9> reasons{ {
            "",
            "usage",
            "pin",
            "busy",
            "notopen",
            "unsupported",
            "range",
            "timeout",
            "failed",
        } };

        constexpr std::array<char, 15> portLetters{ { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'J', 'K', 'L', 'M', 'N', 'P', 'Q' } };
        constexpr const char* hexDigits = "0123456789abcdef";
    }

    Response::Line::Line(Response& response, const char* head, const char* subject)
        : stream(response.StartLine())
    {
        stream << head;

        if (subject != nullptr)
            stream << ' ' << subject;
    }

    Response::Line::~Line()
    {
        stream << "\r\n";
    }

    Response::Line& Response::Line::operator<<(const char* text)
    {
        stream << text;
        return *this;
    }

    Response::Line& Response::Line::operator<<(infra::BoundedConstString text)
    {
        stream << text;
        return *this;
    }

    Response::Line& Response::Line::operator<<(uint32_t value)
    {
        stream << value;
        return *this;
    }

    Response::Line& Response::Line::operator<<(PinId pin)
    {
        stream << 'P' << portLetters[static_cast<std::size_t>(pin.port)] << static_cast<uint32_t>(pin.index);
        return *this;
    }

    Response::Line& Response::Line::Hex(infra::ConstByteRange data)
    {
        for (auto byte : data)
            stream << hexDigits[byte >> 4] << hexDigits[byte & 0x0f];

        return *this;
    }

    Response::Response(services::Tracer& tracer)
        : tracer(tracer)
    {}

    void Response::BeginCommand()
    {
        inCommand = true;
    }

    void Response::EndCommand()
    {
        inCommand = false;
    }

    Response::Line Response::Ok()
    {
        return Line(*this, "OK");
    }

    Response::Line Response::Event(const char* peripheral)
    {
        return Line(*this, "EVT", peripheral);
    }

    void Response::Error(Status status)
    {
        Line(*this, "ERR", reasons[static_cast<std::size_t>(status)]);
    }

    infra::TextOutputStream Response::StartLine()
    {
        auto stream = tracer.Continue();

        // Outside a command the prompt or a partially echoed command may precede the cursor
        if (!inCommand)
            stream << "\r\n";

        return stream;
    }
}
