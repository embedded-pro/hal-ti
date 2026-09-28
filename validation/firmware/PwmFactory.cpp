#include "validation/firmware/PwmFactory.hpp"
#include "BoardProfile.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/EnumCast.hpp"
#include "infra/util/Tokenizer.hpp"
#include "validation/firmware/TivaPinFactory.hpp"

extern "C" uint32_t SystemCoreClock;

namespace validation
{
    namespace
    {
        using services::hil::Choice;
        using services::hil::Status;

        constexpr uint32_t maximumDeadTimeCycles = 4095;
        constexpr uint32_t maximumLoad = 0xffff;
        constexpr uint8_t maximumGenerator = 3;

        constexpr std::array<const char*, 12> openKeys{ { "gens", "pins", "freq", "mode", "div", "dead", "inva", "invb", "update", "trigger", "irq", "sync" } };

        constexpr std::array<Choice<bool>, 2> alignments{ {
            { "edge", false },
            { "center", true },
        } };

        constexpr std::array<Choice<uint8_t>, 7> divisors{ {
            { "1", 0 },
            { "2", 1 },
            { "4", 2 },
            { "8", 3 },
            { "16", 4 },
            { "32", 5 },
            { "64", 6 },
        } };

        constexpr std::array<Choice<bool>, 2> updateModes{ {
            { "local", false },
            { "global", true },
        } };

        constexpr std::array<Choice<PwmTrigger>, 3> triggers{ {
            { "zero", PwmTrigger::zero },
            { "load", PwmTrigger::load },
            { "none", PwmTrigger::none },
        } };

        constexpr std::array<Choice<std::optional<hal::tiva::Pwm::NormalInterruptSource>>, 6> interruptSources{ {
            { "zero", hal::tiva::Pwm::NormalInterruptSource::countZero },
            { "load", hal::tiva::Pwm::NormalInterruptSource::countLoad },
            { "cmpau", hal::tiva::Pwm::NormalInterruptSource::comparatorAUp },
            { "cmpad", hal::tiva::Pwm::NormalInterruptSource::comparatorADown },
            { "cmpbu", hal::tiva::Pwm::NormalInterruptSource::comparatorBUp },
            { "cmpbd", hal::tiva::Pwm::NormalInterruptSource::comparatorBDown },
        } };

        constexpr std::array<Choice<bool>, 2> switches{ {
            { "on", true },
            { "off", false },
        } };

        hal::tiva::PinConfigPeripheral PwmChannelFunction(uint8_t channel)
        {
            return static_cast<hal::tiva::PinConfigPeripheral>(infra::enum_cast(hal::tiva::PinConfigPeripheral::pwmChannel0) + channel);
        }

        std::optional<uint8_t> ChannelOfPin(PinId id, uint8_t module)
        {
            for (uint8_t channel = 0; channel != 2 * (maximumGenerator + 1); ++channel)
                if (SupportsFunction(id, PwmChannelFunction(channel), module))
                    return channel;

            return std::nullopt;
        }

        std::optional<std::optional<PinId>> ParseOptionalPin(infra::BoundedConstString text, const services::hil::PinNaming& naming)
        {
            if (text == "-")
                return std::optional<PinId>();

            if (auto pin = services::hil::ParsePin(text, naming))
                return std::optional<PinId>(*pin);

            return std::nullopt;
        }

        template<class Trigger>
        std::optional<Trigger> ToTrigger(PwmTrigger trigger)
        {
            switch (trigger)
            {
                case PwmTrigger::zero:
                    return Trigger::countZero;
                case PwmTrigger::load:
                    return Trigger::countLoad;
                default:
                    return std::nullopt;
            }
        }

        template<class Driver>
        void StartDriver(Driver& pwm, infra::MemoryRange<const hal::DutyCycle> duties)
        {
            switch (duties.size())
            {
                case 1:
                    pwm.Start(duties[0]);
                    break;
                case 2:
                    pwm.Start(duties[0], duties[1]);
                    break;
                case 3:
                    pwm.Start(duties[0], duties[1], duties[2]);
                    break;
                default:
                    pwm.Start(duties[0], duties[1], duties[2], duties[3]);
                    break;
            }
        }
    }

    TivaPwmFactory::TivaPwmFactory(const services::hil::PinNaming& naming, services::hil::Response& response)
        : naming(naming)
        , response(response)
    {}

    uint8_t TivaPwmFactory::Instances() const
    {
        return board::pwmModules;
    }

    infra::MemoryRange<const char* const> TivaPwmFactory::OpenKeys() const
    {
        return infra::MakeRange(openKeys);
    }

    Status TivaPwmFactory::Prepare(uint8_t module, const services::hil::Arguments& arguments)
    {
        Settings requested;
        return Parse(module, arguments, requested);
    }

    Status TivaPwmFactory::Open(uint8_t module, const services::hil::Arguments& arguments, services::hil::PinOwner& pins, services::hil::PwmHandle*& handle)
    {
        auto& opened = settings.emplace();
        Parse(module, arguments, opened);

        Status status = ClaimPins(pins, opened);
        if (status != Status::done)
        {
            settings = std::nullopt;
            return status;
        }

        for (auto& count : counts)
            count = 0;

        Construct();
        handle = this;
        return Status::done;
    }

    void TivaPwmFactory::ReportOpened(uint8_t, services::hil::Response::Line& line)
    {
        line << " pwmclk=" << PwmClock(settings->divisor);
    }

    Status TivaPwmFactory::ChangeFrequency(uint8_t, uint32_t hertz)
    {
        if (hertz > SystemCoreClock || !ValidFrequency(*settings, hertz))
            return Status::range;

        settings->frequency = hertz;
        return Status::done;
    }

    void TivaPwmFactory::Close(uint8_t, const infra::Function<void()>& onClosed)
    {
        driver.emplace<std::monostate>();
        settings = std::nullopt;
        onClosed();
    }

    std::size_t TivaPwmFactory::Channels() const
    {
        return settings->channels.size();
    }

    void TivaPwmFactory::Start(infra::MemoryRange<const hal::DutyCycle> dutyCycles)
    {
        services::hil::WithDriver(driver, [dutyCycles](auto& pwm)
            {
                StartDriver(pwm, dutyCycles);
            });
    }

    void TivaPwmFactory::SetBaseFrequency(hal::Hertz baseFrequency)
    {
        services::hil::WithDriver(driver, [baseFrequency](auto& pwm)
            {
                pwm.SetBaseFrequency(baseFrequency);
            });
    }

    void TivaPwmFactory::Stop()
    {
        services::hil::WithDriver(driver, [](auto& pwm)
            {
                pwm.Stop();
            });
    }

    Status TivaPwmFactory::Find(const services::hil::Arguments& arguments) const
    {
        uint32_t module = 0;
        Status status = Status::done;
        arguments.NumberAt(0, module, 0, board::pwmModules - 1, status);
        if (status != Status::done)
            return status;

        if (!settings || settings->module != module)
            return Status::notOpen;

        return Status::done;
    }

    Status TivaPwmFactory::EnableFault(bool enable)
    {
        if (settings->synchronous)
            return Status::unsupported;

        settings->fault = enable;
        driver.emplace<std::monostate>();
        Construct();
        return Status::done;
    }

    uint32_t TivaPwmFactory::InterruptCount(uint8_t generator, bool clear)
    {
        auto& count = counts[generator];
        return clear ? count.exchange(0) : count.load();
    }

    Status TivaPwmFactory::Parse(uint8_t module, const services::hil::Arguments& arguments, Settings& requested) const
    {
        requested.module = module;
        requested.trigger = board::pwmTrigger;
        requested.synchronous = board::pwmSynchronous;

        Status status = Status::done;
        arguments.Number("freq", requested.frequency, 1, SystemCoreClock, status);
        arguments.Select("mode", requested.centerAligned, alignments, status);
        arguments.Select("div", requested.divisor, divisors, status);
        arguments.Flag("inva", requested.invertA, status);
        arguments.Flag("invb", requested.invertB, status);
        arguments.Select("update", requested.globalUpdate, updateModes, status);
        arguments.Select("trigger", requested.trigger, triggers, status);
        arguments.Select("irq", requested.interrupt, interruptSources, status);
        arguments.Flag("sync", requested.synchronous, status);

        if (status == Status::done && arguments.Has("dead"))
        {
            if (arguments.Key("dead") == "off")
                requested.deadTime = std::nullopt;
            else
            {
                uint32_t deadTime = 0;
                arguments.Number("dead", deadTime, 0, 1000000, status);
                requested.deadTime = deadTime;
            }
        }

        if (status != Status::done)
            return status;

        if (requested.deadTime && static_cast<uint64_t>(*requested.deadTime) * PwmClock(requested.divisor) / 1000000000u > maximumDeadTimeCycles)
            return Status::range;

        if (!ValidFrequency(requested, requested.frequency))
            return Status::range;

        if (requested.synchronous && requested.interrupt)
            return Status::unsupported;

        return ParseChannels(arguments, requested);
    }

    Status TivaPwmFactory::ParseChannels(const services::hil::Arguments& arguments, Settings& requested) const
    {
        auto generators = arguments.Key("gens");
        auto pins = arguments.Key("pins");

        if (!generators && !pins)
        {
            if (requested.module != board::pwmModule)
                return Status::usage;

            for (const auto& phase : board::pwmPhases)
                requested.channels.push_back(Channel{ phase.generator, phase.a, phase.b });

            return Status::done;
        }

        infra::Tokenizer generatorTokens(generators.value_or(infra::BoundedConstString()), ',');
        infra::Tokenizer pinTokens(pins.value_or(infra::BoundedConstString()), ',');
        const auto count = generators ? generatorTokens.Size() : pinTokens.Size();

        if (count == 0 || count > maximumChannels || (generators && pins && generatorTokens.Size() != pinTokens.Size()))
            return Status::usage;

        for (std::size_t i = 0; i != count; ++i)
        {
            Channel channel{};

            if (pins)
            {
                infra::Tokenizer pair(pinTokens.Token(i), ':');
                if (pair.Size() != 2)
                    return Status::usage;

                auto a = ParseOptionalPin(pair.Token(0), naming);
                auto b = ParseOptionalPin(pair.Token(1), naming);
                if (!a || !b)
                    return Status::pin;

                if (!*a && !*b)
                    return Status::usage;

                channel.a = *a;
                channel.b = *b;
            }

            if (generators)
            {
                auto generator = services::hil::ParseNumber(generatorTokens.Token(i));
                if (!generator)
                    return Status::usage;

                if (*generator > maximumGenerator)
                    return Status::range;

                channel.generator = static_cast<uint8_t>(*generator);
            }
            else
            {
                auto pinChannel = ChannelOfPin(channel.a ? *channel.a : *channel.b, requested.module);
                if (!pinChannel)
                    return Status::pin;

                channel.generator = *pinChannel / 2;
            }

            if (!pins)
            {
                channel.a = FindFunctionPin(PwmChannelFunction(2 * channel.generator), requested.module);
                channel.b = FindFunctionPin(PwmChannelFunction(2 * channel.generator + 1), requested.module);
                if (!channel.a && !channel.b)
                    return Status::unsupported;
            }

            for (const auto& other : requested.channels)
                if (other.generator == channel.generator)
                    return Status::usage;

            requested.channels.push_back(channel);
        }

        return Status::done;
    }

    Status TivaPwmFactory::ClaimPins(services::hil::PinOwner& pins, Settings& opened)
    {
        for (auto& channel : opened.channels)
        {
            Status status = pins.ClaimFunction(channel.a, Function(PwmChannelFunction(2 * channel.generator)), opened.module, channel.pinA);
            if (status == Status::done)
                status = pins.ClaimFunction(channel.b, Function(PwmChannelFunction(2 * channel.generator + 1)), opened.module, channel.pinB);

            if (status != Status::done)
                return status;
        }

        return Status::done;
    }

    void TivaPwmFactory::Construct()
    {
        const auto& opened = *settings;
        const auto deadTimeCycles = static_cast<uint16_t>(static_cast<uint64_t>(opened.deadTime.value_or(0)) * PwmClock(opened.divisor) / 1000000000u);

        auto fillConfig = [&opened, deadTimeCycles](auto& config)
        {
            using Config = std::decay_t<decltype(config)>;

            config.channelAInverted = opened.invertA;
            config.channelBInverted = opened.invertB;
            config.control.mode = opened.centerAligned ? Config::Control::Mode::centerAligned : Config::Control::Mode::edgeAligned;
            config.control.updateMode = opened.globalUpdate ? Config::Control::UpdateMode::globally : Config::Control::UpdateMode::locally;
            config.control.debugMode = false;
            config.clockDivisor = static_cast<typename Config::ClockDivisor>(opened.divisor);

            if (opened.deadTime)
                config.deadTime = typename Config::DeadTime{ deadTimeCycles, deadTimeCycles };
            else
                config.deadTime = std::nullopt;
        };

        auto makeChannels = [&opened](auto& channels)
        {
            using PinChannel = typename std::decay_t<decltype(channels)>::value_type;

            for (std::size_t i = 0; i != opened.channels.size(); ++i)
            {
                const auto& channel = opened.channels[i];
                channels.push_back(PinChannel{ static_cast<decltype(PinChannel::generator)>(channel.generator), PinOrDummy(channel.pinA), PinOrDummy(channel.pinB),
                    channel.pinA != nullptr, channel.pinB != nullptr, i == 0 ? ToTrigger<typename PinChannel::Trigger>(opened.trigger) : std::nullopt });
            }
        };

        if (opened.synchronous)
        {
            syncConfig = hal::tiva::SynchronousPwm::Config{};
            fillConfig(syncConfig);

            infra::BoundedVector<hal::tiva::SynchronousPwm::PinChannel>::WithMaxSize<maximumChannels> channels;
            makeChannels(channels);
            driver.emplace<hal::tiva::SynchronousPwm>(opened.module, infra::MakeRange(channels), syncConfig);
        }
        else
        {
            asyncConfig = hal::tiva::Pwm::Config{};
            fillConfig(asyncConfig);

            if (opened.interrupt || opened.fault)
            {
                hal::tiva::Pwm::Config::InterruptConfig interrupts;
                interrupts.priority = opened.fault ? hal::cortex::InterruptPriority::highest : hal::cortex::InterruptPriority::normal;

                for (const auto& channel : opened.channels)
                {
                    const auto generator = static_cast<hal::tiva::Pwm::GeneratorIndex>(channel.generator);

                    if (opened.interrupt)
                        interrupts.normalSources.push_back({ generator, *opened.interrupt });

                    if (opened.fault)
                        interrupts.faultConfigs.push_back({ generator, uint8_t{ 0 },
                            static_cast<uint8_t>(infra::enum_cast(hal::tiva::Pwm::FaultInputComparator::comparator0) | infra::enum_cast(hal::tiva::Pwm::FaultInputComparator::comparator1)),
                            true, uint16_t{ 0 } });
                }

                asyncConfig.interruptConfig = interrupts;
            }

            infra::BoundedVector<hal::tiva::Pwm::PinChannel>::WithMaxSize<maximumChannels> channels;
            makeChannels(channels);
            driver.emplace<hal::tiva::Pwm>(
                opened.module, infra::MakeRange(channels), asyncConfig,
                [this](hal::tiva::Pwm::NormalEvent event)
                {
                    counts[infra::enum_cast(event.generator)].fetch_add(1, std::memory_order_relaxed);
                },
                [this](hal::tiva::Pwm::FaultEvent event)
                {
                    OnFault(event);
                });
        }

        SetBaseFrequency(hal::Hertz(opened.frequency));
    }

    void TivaPwmFactory::OnFault(const hal::tiva::Pwm::FaultEvent& event)
    {
        uint8_t inputs = 0;
        for (auto comparator : event.comparatorInputsByGenerator)
            inputs |= infra::enum_cast(comparator);

        faultInputs.fetch_or(inputs, std::memory_order_relaxed);

        if (!faultReportPending.exchange(true))
            infra::EventDispatcher::Instance().Schedule([this]()
                {
                    ReportFault();
                });
    }

    void TivaPwmFactory::ReportFault()
    {
        faultReportPending = false;
        auto inputs = faultInputs.exchange(0);

        if (settings)
            response.Event("pwm") << " module=" << settings->module << " fault=" << inputs;
    }

    uint32_t TivaPwmFactory::PwmClock(uint8_t divisor) const
    {
        return SystemCoreClock >> divisor;
    }

    bool TivaPwmFactory::ValidFrequency(const Settings& opened, uint32_t frequency) const
    {
        const auto period = PwmClock(opened.divisor) / frequency;
        const auto load = opened.centerAligned ? period / 2 : period - 1;
        return period != 0 && load != 0 && load <= maximumLoad;
    }

    PwmExtensionCommands::PwmExtensionCommands(services::hil::Context& context, TivaPwmFactory& factory)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , factory(factory)
        , commands{ {
              services::hil::Bind<PwmExtensionCommands, &PwmExtensionCommands::Fault>("pwm.fault", "<module> <on|off>", *this, context.response),
              services::hil::Bind<PwmExtensionCommands, &PwmExtensionCommands::Count>("pwm.count", "<module> <gen> [clear=]", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> PwmExtensionCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    Status PwmExtensionCommands::Fault(const services::hil::Arguments& arguments)
    {
        if (!arguments.Shape(2, 2, {}))
            return Status::usage;

        if (!board::hasFaultComparators)
            return Status::unsupported;

        bool enable = false;
        Status status = factory.Find(arguments);
        arguments.SelectAt(1, enable, switches, status);
        if (status != Status::done)
            return status;

        status = factory.EnableFault(enable);
        if (status != Status::done)
            return status;

        context.response.Ok();
        return Status::done;
    }

    Status PwmExtensionCommands::Count(const services::hil::Arguments& arguments)
    {
        if (!arguments.Shape(2, 2, { "clear" }))
            return Status::usage;

        uint32_t generator = 0;
        bool clear = false;
        Status status = factory.Find(arguments);
        arguments.NumberAt(1, generator, 0, maximumGenerator, status);
        arguments.Flag("clear", clear, status);
        if (status != Status::done)
            return status;

        context.response.Ok() << " count=" << factory.InterruptCount(static_cast<uint8_t>(generator), clear);
        return Status::done;
    }
}
