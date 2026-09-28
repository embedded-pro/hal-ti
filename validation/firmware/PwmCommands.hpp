#ifndef VALIDATION_PWM_COMMANDS_HPP
#define VALIDATION_PWM_COMMANDS_HPP

#include "hal_tiva/synchronous_tiva/SynchronousPwm.hpp"
#include "hal_tiva/tiva/Pwm.hpp"
#include "infra/util/BoundedVector.hpp"
#include "services/util/Terminal.hpp"
#include "validation/firmware/Context.hpp"
#include <array>
#include <atomic>
#include <optional>
#include <variant>

namespace validation
{
    class PwmCommands
        : public services::TerminalCommands
    {
    public:
        explicit PwmCommands(Context& context);

        infra::MemoryRange<const Command> Commands() override;

    private:
        struct Channel
        {
            uint8_t generator;
            std::optional<PinId> a;
            std::optional<PinId> b;
            hal::tiva::GpioPin* pinA = nullptr;
            hal::tiva::GpioPin* pinB = nullptr;
        };

        struct Settings
        {
            uint8_t module = 0;
            infra::BoundedVector<Channel>::WithMaxSize<4> channels;
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

        Status Open(const Arguments& arguments);
        Status Fault(const Arguments& arguments);
        Status Duty(const Arguments& arguments);
        Status Frequency(const Arguments& arguments);
        Status Stop(const Arguments& arguments);
        Status Count(const Arguments& arguments);
        Status Close(const Arguments& arguments);

        Status ParseChannels(const Arguments& arguments, Settings& settings) const;
        Status ClaimPins(Settings& settings);
        Status Find(const Arguments& arguments);
        void Construct();
        void Destroy();
        void OnFault(const hal::tiva::Pwm::FaultEvent& event);
        void ReportFault();
        uint32_t PwmClock(uint8_t divisor) const;
        bool ValidFrequency(const Settings& settings, uint32_t frequency) const;

        template<class Driver>
        void Start(Driver& driver, infra::MemoryRange<const hal::DutyCycle> duties);

    private:
        Context& context;
        std::optional<Settings> settings;
        hal::tiva::Pwm::Config asyncConfig;
        hal::tiva::SynchronousPwm::Config syncConfig;
        std::variant<std::monostate, hal::tiva::Pwm, hal::tiva::SynchronousPwm> driver;
        std::array<std::atomic<uint32_t>, 4> counts{};
        std::atomic<uint8_t> faultInputs{ 0 };
        std::atomic<bool> faultReportPending{ false };
        std::array<Command, 7> commands;
    };
}

#endif
