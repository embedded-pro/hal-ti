#ifndef VALIDATION_CONTEXT_HPP
#define VALIDATION_CONTEXT_HPP

#include "hal_tiva/tiva/Dma.hpp"
#include "services/util/Terminal.hpp"
#include "validation/firmware/Arguments.hpp"
#include "validation/firmware/PinPool.hpp"
#include "validation/firmware/Response.hpp"
#include <type_traits>
#include <variant>

namespace validation
{
    struct Context
    {
        Response& response;
        PinPool& pins;
        hal::tiva::Dma& dma;
        services::TerminalWithCommands& terminal;
    };

    template<class Group, Status (Group::*Method)(const Arguments&)>
    services::TerminalCommands::Command Bind(const char* name, const char* usage, Group& group, Response& response)
    {
        return { { name, name, usage }, [&group, &response](const infra::BoundedConstString& parameters)
            {
                Status status = (group.*Method)(Arguments(parameters));

                if (status != Status::done)
                    response.Error(status);
            } };
    }

    template<class... Drivers, class F>
    void WithDriver(std::variant<std::monostate, Drivers...>& driver, F&& f)
    {
        std::visit([&f](auto& alternative)
            {
                if constexpr (!std::is_same_v<std::decay_t<decltype(alternative)>, std::monostate>)
                    f(alternative);
            },
            driver);
    }
}

#endif
