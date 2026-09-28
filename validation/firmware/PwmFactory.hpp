#ifndef VALIDATION_PWM_FACTORY_HPP
#define VALIDATION_PWM_FACTORY_HPP

#include "hal_tiva/synchronous_tiva/SynchronousPwm.hpp"
#include "hal_tiva/tiva/Pwm.hpp"
#include "infra/event/AtomicTriggerScheduler.hpp"
#include "infra/util/BoundedVector.hpp"
#include "services/hil/HilPinPool.hpp"
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
    {
    public:
        TivaPwmFactory(const services::HilPinNaming& naming, services::HilResponse& response, services::HilPinPool& pins);

        uint8_t Instances() const override;
        infra::MemoryRange<const char* const> OpenKeys() const override;
        services::HilStatus Prepare(uint8_t module, const services::HilArguments& arguments) override;
        services::HilStatus Open(uint8_t module, const services::HilArguments& arguments, services::HilPinOwner& pins, services::HilPwmHandle*& handle) override;
        void ReportOpened(uint8_t module, services::HilResponse::Line& line) override;
        services::HilStatus ChangeFrequency(uint8_t module, uint32_t hertz) override;
        void Close(uint8_t module, const infra::Function<void()>& onClosed) override;

        services::HilStatus Find(const services::HilArguments& arguments) const;
        services::HilStatus ConfigureFault(const services::HilArguments& arguments);
        uint32_t InterruptCount(uint8_t generator, bool clear);

    private:
        static constexpr std::size_t maximumChannels = services::HilPwmCommands::maximumChannels;

        using Source = hal::tiva::Pwm::NormalInterruptSource;

        struct Channel
        {
            uint8_t generator;
            std::optional<HilPinId> a;
            std::optional<HilPinId> b;
            hal::GpioPin* pinA = nullptr;
            hal::GpioPin* pinB = nullptr;
            std::optional<Source> trigger;
            std::optional<Source> interrupt;
        };

        struct DeadTime
        {
            uint32_t rise;
            uint32_t fall;
        };

        struct Fault
        {
            uint8_t generators = 0;
            uint8_t comparators = 0;
            uint8_t inputs = 0;
            bool latch = false;
            uint16_t minimumPeriod = 0;
        };

        struct Settings
        {
            uint8_t module = 0;
            infra::BoundedVector<Channel>::WithMaxSize<maximumChannels> channels;
            uint32_t frequency = 10000;
            bool centerAligned = false;
            uint8_t divisor = 0;
            std::optional<DeadTime> deadTime;
            bool invertA = false;
            bool invertB = false;
            bool globalUpdate = false;
            bool synchronous = false;
            std::optional<Fault> fault;
        };

        services::HilStatus Parse(uint8_t module, const services::HilArguments& arguments, Settings& settings) const;
        services::HilStatus ParseChannels(const services::HilArguments& arguments, Settings& settings) const;
        services::HilStatus ParseFault(const services::HilArguments& arguments, std::optional<Fault>& fault, std::optional<HilPinId>& pin) const;
        services::HilStatus ClaimPins(services::HilPinOwner& pins, Settings& settings);
        services::HilPwmHandle& Construct();
        template<class Driver>
        services::HilPwmHandle& Adapt(Driver& pwm);
        void ReleaseFaultPin();
        void OnFault(const hal::tiva::Pwm::FaultEvent& event);
        void ReportFault();
        uint32_t PwmClock(uint8_t divisor) const;
        uint32_t Cycles(uint32_t nanoseconds, uint8_t divisor) const;
        bool ValidFrequency(const Settings& settings, uint32_t frequency) const;

    private:
        const services::HilPinNaming& naming;
        services::HilResponse& response;
        services::HilPinOwner faultPins;
        std::optional<hal::tiva::PeripheralPin> faultPin;
        std::optional<Settings> settings;
        hal::tiva::Pwm::Config asyncConfig;
        hal::tiva::SynchronousPwm::Config syncConfig;
        std::variant<std::monostate, hal::tiva::Pwm, hal::tiva::SynchronousPwm> driver;
        std::variant<std::monostate, services::HilPwmAdapter<hal::tiva::Pwm>, services::HilPwmAdapter<hal::tiva::SynchronousPwm>> adapter;
        std::array<std::atomic<uint32_t>, 4> counts{};
        std::atomic<uint8_t> faultGenerators{ 0 };
        std::atomic<uint8_t> faultComparators{ 0 };
        std::atomic<uint8_t> faultInputs{ 0 };
        infra::AtomicTriggerScheduler faultReport;
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
