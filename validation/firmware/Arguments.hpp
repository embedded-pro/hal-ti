#ifndef VALIDATION_ARGUMENTS_HPP
#define VALIDATION_ARGUMENTS_HPP

#include "hal/interfaces/DutyCycle.hpp"
#include "hal_tiva/tiva/Gpio.hpp"
#include "infra/util/BoundedString.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Tokenizer.hpp"
#include "validation/firmware/BoardTypes.hpp"
#include "validation/firmware/Response.hpp"
#include <array>
#include <cstdint>
#include <initializer_list>
#include <optional>

namespace validation
{
    template<class T>
    struct Choice
    {
        const char* name;
        T value;
    };

    std::optional<uint32_t> ParseNumber(infra::BoundedConstString text);
    std::optional<hal::DutyCycle> ParseDutyCycle(infra::BoundedConstString text);
    std::optional<PinId> ParsePin(infra::BoundedConstString text, hal::tiva::Drive* aliasPull = nullptr);
    Status ParseHex(infra::BoundedConstString text, infra::ByteRange output, std::size_t& size);

    template<class T, std::size_t N>
    std::optional<T> ParseChoice(infra::BoundedConstString text, const std::array<Choice<T>, N>& choices)
    {
        for (const auto& choice : choices)
            if (text == choice.name)
                return choice.value;

        return std::nullopt;
    }

    // Every accessor leaves status untouched once it holds an error, so a handler can parse all its arguments and check once
    class Arguments
    {
    public:
        explicit Arguments(infra::BoundedConstString parameters);

        bool Shape(std::size_t minimumPositional, std::size_t maximumPositional, std::initializer_list<const char*> keys) const;
        std::size_t PositionalCount() const;
        infra::BoundedConstString Positional(std::size_t index) const;
        std::optional<infra::BoundedConstString> Key(const char* key) const;
        bool Has(const char* key) const;

        void NumberAt(std::size_t index, uint32_t& value, uint32_t minimum, uint32_t maximum, Status& status) const;
        void Number(const char* key, uint32_t& value, uint32_t minimum, uint32_t maximum, Status& status) const;
        void Flag(const char* key, bool& value, Status& status) const;
        void Pin(const char* key, std::optional<PinId>& pin, Status& status) const;
        void PinAt(std::size_t index, PinId& pin, Status& status, hal::tiva::Drive* aliasPull = nullptr) const;

        template<class T, std::size_t N>
        void Select(const char* key, T& value, const std::array<Choice<T>, N>& choices, Status& status) const
        {
            if (status != Status::done)
                return;

            if (auto text = Key(key))
                Select(*text, value, choices, status);
        }

        template<class T, std::size_t N>
        void SelectAt(std::size_t index, T& value, const std::array<Choice<T>, N>& choices, Status& status) const
        {
            if (status == Status::done)
                Select(Positional(index), value, choices, status);
        }

    private:
        template<class T, std::size_t N>
        static void Select(infra::BoundedConstString text, T& value, const std::array<Choice<T>, N>& choices, Status& status)
        {
            if (auto choice = ParseChoice(text, choices))
                value = *choice;
            else
                status = Status::usage;
        }

        static void Number(infra::BoundedConstString text, uint32_t& value, uint32_t minimum, uint32_t maximum, Status& status);
        static bool IsKeyValue(infra::BoundedConstString token);

    private:
        infra::Tokenizer tokenizer;
        std::size_t tokens;
    };
}

#endif
