#ifndef VALIDATION_PWM_FACTORY_HPP
#define VALIDATION_PWM_FACTORY_HPP

#include "hal_tiva/synchronous_tiva/SynchronousPwm.hpp"
#include "hal_tiva/tiva/Pwm.hpp"
#include "infra/util/BoundedVector.hpp"
#include "services/hil/commands/HilPwmCommands.hpp"
#include "validation/firmware/BoardTypes.hpp"
#include <array>
#include <atomic>
#include <optional>
#include <variant>

namespace validation
{
    class TivaPwmFactory
        : public services::HilPwmFactory
        , public services::HilPwmHandle
    {
    public:
        TivaPwmFactory(const services::HilPinNaming& naming, services::HilResponse& response);

        uint8_t Instances() const override;
        infra::MemoryRange<const char* const> OpenKeys() const override;
        services::HilStatus Prepare(uint8_t module, const services::HilArguments& arguments) override;
        services::HilStatus Open(uint8_t module, const services::HilArguments& arguments, services::HilPinOwner& pins, services::HilPwmHandle*& handle) override;
        void ReportOpened(uint8_t module, services::HilResponse::Line& line) override;
        services::HilStatus ChangeFrequency(uint8_t module, uint32_t hertz) override;
        void Close(uint8_t module, const infra::Function<void()>& onClosed) override;

        std::size_t Channels() const override;
        void Start(infra::MemoryRange<const hal::DutyCycle> dutyCycles) override;
        void SetBaseFrequency(hal::Hertz baseFrequency) override;
        void Stop() override;

        services::HilStatus Find(const services::HilArguments& arguments) const;
        services::HilStatus EnableFault(bool enable);
        uint32_t InterruptCount(uint8_t generator, bool clear);

    private:
        static constexpr std::size_t maximumChannels = services::HilPwmCommands::maximumChannels;

        struct Channel
        {
            uint8_t generator;
            std::optional<HilPinId> a;
            std::optional<HilPinId> b;
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

        services::HilStatus Parse(uint8_t module, const services::HilArguments& arguments, Settings& settings) const;
        services::HilStatus ParseChannels(const services::HilArguments& arguments, Settings& settings) const;
        services::HilStatus ClaimPins(services::HilPinOwner& pins, Settings& settings);
        void Construct();
        void OnFault(const hal::tiva::Pwm::FaultEvent& event);
        void ReportFault();
        uint32_t PwmClock(uint8_t divisor) const;
        bool ValidFrequency(const Settings& settings, uint32_t frequency) const;

    private:
        const services::HilPinNaming& naming;
        services::HilResponse& response;
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
        PwmExtensionCommands(services::HilContext& context, TivaPwmFactory& factory);

        infra::MemoryRange<const Command> Commands() override;

    private:
        services::HilStatus Fault(const services::HilArguments& arguments);
        services::HilStatus Count(const services::HilArguments& arguments);

    private:
        services::HilContext& context;
        TivaPwmFactory& factory;
        std::array<Command, 2> commands;
    };
}

#endif
