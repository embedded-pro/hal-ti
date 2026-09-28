#ifndef VALIDATION_PWM_FACTORY_HPP
#define VALIDATION_PWM_FACTORY_HPP

#include "hal_tiva/synchronous_tiva/SynchronousPwm.hpp"
#include "hal_tiva/tiva/Pwm.hpp"
#include "infra/util/BoundedVector.hpp"
#include "services/hil/commands/PwmCommands.hpp"
#include "validation/firmware/BoardTypes.hpp"
#include <array>
#include <atomic>
#include <optional>
#include <variant>

namespace validation
{
    class TivaPwmFactory
        : public services::hil::PwmFactory
        , public services::hil::PwmHandle
    {
    public:
        TivaPwmFactory(const services::hil::PinNaming& naming, services::hil::Response& response);

        uint8_t Instances() const override;
        infra::MemoryRange<const char* const> OpenKeys() const override;
        services::hil::Status Prepare(uint8_t module, const services::hil::Arguments& arguments) override;
        services::hil::Status Open(uint8_t module, const services::hil::Arguments& arguments, services::hil::PinOwner& pins, services::hil::PwmHandle*& handle) override;
        void ReportOpened(uint8_t module, services::hil::Response::Line& line) override;
        services::hil::Status ChangeFrequency(uint8_t module, uint32_t hertz) override;
        void Close(uint8_t module, const infra::Function<void()>& onClosed) override;

        std::size_t Channels() const override;
        void Start(infra::MemoryRange<const hal::DutyCycle> dutyCycles) override;
        void SetBaseFrequency(hal::Hertz baseFrequency) override;
        void Stop() override;

        services::hil::Status Find(const services::hil::Arguments& arguments) const;
        services::hil::Status EnableFault(bool enable);
        uint32_t InterruptCount(uint8_t generator, bool clear);

    private:
        static constexpr std::size_t maximumChannels = services::hil::PwmCommands::maximumChannels;

        struct Channel
        {
            uint8_t generator;
            std::optional<PinId> a;
            std::optional<PinId> b;
            hal::GpioPin* pinA = nullptr;
            hal::GpioPin* pinB = nullptr;
        };

        struct Settings
        {
            uint8_t module = 0;
            infra::BoundedVector<Channel>::WithMaxSize<maximumChannels> channels;
            uint32_t frequency = 20000;
            bool centerAligned = true;
            uint8_t divisor = 1;
            std::optional<uint32_t> deadTime = 1000;
            bool invertA = false;
            bool invertB = false;
            bool globalUpdate = true;
            PwmTrigger trigger = PwmTrigger::none;
            std::optional<hal::tiva::Pwm::NormalInterruptSource> interrupt;
            bool synchronous = false;
            bool fault = false;
        };

        services::hil::Status Parse(uint8_t module, const services::hil::Arguments& arguments, Settings& settings) const;
        services::hil::Status ParseChannels(const services::hil::Arguments& arguments, Settings& settings) const;
        services::hil::Status ClaimPins(services::hil::PinOwner& pins, Settings& settings);
        void Construct();
        void OnFault(const hal::tiva::Pwm::FaultEvent& event);
        void ReportFault();
        uint32_t PwmClock(uint8_t divisor) const;
        bool ValidFrequency(const Settings& settings, uint32_t frequency) const;

    private:
        const services::hil::PinNaming& naming;
        services::hil::Response& response;
        std::optional<Settings> settings;
        hal::tiva::Pwm::Config asyncConfig;
        hal::tiva::SynchronousPwm::Config syncConfig;
        std::variant<std::monostate, hal::tiva::Pwm, hal::tiva::SynchronousPwm> driver;
        std::array<std::atomic<uint32_t>, 4> counts{};
        std::atomic<uint8_t> faultInputs{ 0 };
        std::atomic<bool> faultReportPending{ false };
    };

    class PwmExtensionCommands
        : public services::TerminalCommands
    {
    public:
        PwmExtensionCommands(services::hil::Context& context, TivaPwmFactory& factory);

        infra::MemoryRange<const Command> Commands() override;

    private:
        services::hil::Status Fault(const services::hil::Arguments& arguments);
        services::hil::Status Count(const services::hil::Arguments& arguments);

    private:
        services::hil::Context& context;
        TivaPwmFactory& factory;
        std::array<Command, 2> commands;
    };
}

#endif
