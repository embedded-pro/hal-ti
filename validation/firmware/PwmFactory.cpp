#include "validation/firmware/PwmFactory.hpp"
#include "BoardProfile.hpp"
#include "infra/util/EnumCast.hpp"
#include "infra/util/Tokenizer.hpp"
#include "validation/firmware/TivaPinFactory.hpp"
#include <algorithm>
#include DEVICE_HEADER

extern "C" uint32_t SystemCoreClock;

namespace validation
{
    namespace
    {
        using services::HilChoice;
        using services::HilStatus;

        constexpr uint32_t maximumDeadTimeCycles = 4095;
        constexpr uint32_t maximumDeadTimeNs = 1000000;
        constexpr uint32_t maximumLoad = 0xffff;
        constexpr uint8_t maximumGenerator = 3;
        constexpr uint32_t maximumComparators = 0xff;
        constexpr uint32_t maximumFaultInputs = 0x0f;
        constexpr uint32_t maximumFaultPeriod = 0xffff;

        constexpr std::array<const char*, 12> openKeys{ { "gens", "pins", "freq", "mode", "div", "dead", "inva", "invb", "update", "trigger", "irq", "sync" } };
        constexpr std::array<const char*, 6> faultKeys{ { "gens", "comparators", "inputs", "pin", "latch", "minperiod" } };

        constexpr std::array<HilChoice<bool>, 2> alignments{ {
            { "edge", false },
            { "center", true },
        } };

        constexpr std::array<HilChoice<uint8_t>, 7> divisors{ {
            { "1", 0 },
            { "2", 1 },
            { "4", 2 },
            { "8", 3 },
            { "16", 4 },
            { "32", 5 },
            { "64", 6 },
        } };

        constexpr std::array<HilChoice<bool>, 2> updateModes{ {
            { "local", false },
            { "global", true },
        } };

        constexpr std::array<HilChoice<std::optional<hal::tiva::Pwm::NormalInterruptSource>>, 7> sources{ {
            { "none", std::nullopt },
            { "zero", hal::tiva::Pwm::NormalInterruptSource::countZero },
            { "load", hal::tiva::Pwm::NormalInterruptSource::countLoad },
            { "cmpau", hal::tiva::Pwm::NormalInterruptSource::comparatorAUp },
            { "cmpad", hal::tiva::Pwm::NormalInterruptSource::comparatorADown },
            { "cmpbu", hal::tiva::Pwm::NormalInterruptSource::comparatorBUp },
            { "cmpbd", hal::tiva::Pwm::NormalInterruptSource::comparatorBDown },
        } };

        constexpr std::array<HilChoice<bool>, 2> switches{ {
            { "on", true },
            { "off", false },
        } };

        hal::tiva::PinConfigPeripheral PwmChannelFunction(uint8_t channel)
        {
            return static_cast<hal::tiva::PinConfigPeripheral>(infra::enum_cast(hal::tiva::PinConfigPeripheral::pwmChannel0) + channel);
        }

        std::optional<uint8_t> ChannelOfPin(HilPinId id, uint8_t module)
        {
            for (uint8_t channel = 0; channel != 2 * (maximumGenerator + 1); ++channel)
                if (SupportsFunction(id, PwmChannelFunction(channel), module))
                    return channel;

            return std::nullopt;
        }

        std::optional<std::optional<HilPinId>> ParseOptionalPin(infra::BoundedConstString text, const services::HilPinNaming& naming)
        {
            if (text == "-")
                return std::optional<HilPinId>();

            if (auto pin = services::HilArguments::ParsePin(text, naming))
                return std::optional<HilPinId>(*pin);

            return std::nullopt;
        }

        HilStatus ParseNumbers(infra::BoundedConstString text, uint32_t maximum, infra::MemoryRange<uint32_t> values, std::size_t& count)
        {
            infra::Tokenizer tokens(text, ',');
            count = tokens.Size();
            if (count == 0 || count > values.size())
                return HilStatus::usage;

            for (std::size_t i = 0; i != count; ++i)
            {
                auto value = services::HilArguments::ParseNumber(tokens.Token(i));
                if (!value)
                    return HilStatus::usage;

                if (*value > maximum)
                    return HilStatus::range;

                values[i] = *value;
            }

            return HilStatus::done;
        }

        template<class Channels, class Member>
        HilStatus ParseSources(const services::HilArguments& arguments, const char* key, Channels& channels, Member member)
        {
            auto text = arguments.Key(key);
            if (!text)
                return HilStatus::done;

            infra::Tokenizer tokens(*text, ',');
            if (tokens.Size() != 1 && tokens.Size() != channels.size())
                return HilStatus::usage;

            for (std::size_t i = 0; i != channels.size(); ++i)
            {
                auto source = services::HilArguments::ParseChoice(tokens.Token(tokens.Size() == 1 ? 0 : i), sources);
                if (!source)
                    return HilStatus::usage;

                channels[i].*member = *source;
            }

            return HilStatus::done;
        }

        // The driver ORs its settings into the generator registers and leaves them behind when destroyed, so a new instance starts from a reset module
        void ResetPwmModule(uint8_t module)
        {
            SYSCTL->RCGCPWM |= 1u << module;
            while ((SYSCTL->PRPWM & (1u << module)) == 0)
            {
            }

            SYSCTL->SRPWM |= 1u << module;
            SYSCTL->SRPWM &= ~(1u << module);
        }
    }

    TivaPwmFactory::TivaPwmFactory(const services::HilPinNaming& naming, services::HilResponse& response, services::HilPinPool& pins)
        : naming(naming)
        , response(response)
        , faultPins(pins, services::HilOwners::extension)
    {}

    uint8_t TivaPwmFactory::Instances() const
    {
        return board::pwmModules;
    }

    infra::MemoryRange<const char* const> TivaPwmFactory::OpenKeys() const
    {
        return infra::MakeRange(openKeys);
    }

    HilStatus TivaPwmFactory::Prepare(uint8_t module, const services::HilArguments& arguments)
    {
        Settings requested;
        return Parse(module, arguments, requested);
    }

    HilStatus TivaPwmFactory::Open(uint8_t module, const services::HilArguments& arguments, services::HilPinOwner& pins, services::HilPwmHandle*& handle)
    {
        auto& opened = settings.emplace();
        Parse(module, arguments, opened);

        HilStatus status = ClaimPins(pins, opened);
        if (status != HilStatus::done)
        {
            settings = std::nullopt;
            return status;
        }

        for (auto& count : counts)
            count = 0;

        handle = &Construct();
        return HilStatus::done;
    }

    void TivaPwmFactory::ReportOpened(uint8_t, services::HilResponse::Line& line)
    {
        line << " pwmclk=" << PwmClock(settings->divisor);
    }

    HilStatus TivaPwmFactory::ChangeFrequency(uint8_t, uint32_t hertz)
    {
        if (hertz > SystemCoreClock || !ValidFrequency(*settings, hertz))
            return HilStatus::range;

        settings->frequency = hertz;
        return HilStatus::done;
    }

    void TivaPwmFactory::Close(uint8_t, const infra::Function<void()>& onClosed)
    {
        adapter.emplace<std::monostate>();
        driver.emplace<std::monostate>();
        ReleaseFaultPin();
        settings = std::nullopt;
        onClosed();
    }

    HilStatus TivaPwmFactory::Find(const services::HilArguments& arguments) const
    {
        uint32_t module = 0;
        HilStatus status = HilStatus::done;
        arguments.NumberAt(0, module, 0, board::pwmModules - 1, status);
        if (status != HilStatus::done)
            return status;

        if (!settings || settings->module != module)
            return HilStatus::notOpen;

        return HilStatus::done;
    }

    HilStatus TivaPwmFactory::ConfigureFault(const services::HilArguments& arguments)
    {
        std::optional<Fault> fault;
        std::optional<HilPinId> pin;
        HilStatus status = ParseFault(arguments, fault, pin);
        if (status != HilStatus::done)
            return status;

        if (settings->synchronous)
            return HilStatus::unsupported;

        hal::GpioPin* claimed = nullptr;
        const bool newPin = pin.has_value() && pin != faultPinId;
        if (newPin)
        {
            status = faultPins.ClaimFunction(*pin, Function(hal::tiva::PinConfigPeripheral::pwmFault), settings->module, claimed);
            if (status != HilStatus::done)
                return status;
        }

        adapter.emplace<std::monostate>();
        driver.emplace<std::monostate>();

        if (faultPinId.has_value() && pin != faultPinId)
        {
            faultPin.reset();
            faultPins.Release(*faultPinId);
            faultPinId = std::nullopt;
        }

        if (newPin)
        {
            faultPin.emplace(PinOrDummy(claimed), hal::tiva::PinConfigPeripheral::pwmFault);
            faultPinId = pin;
        }

        settings->fault = fault;
        Construct();
        return HilStatus::done;
    }

    uint32_t TivaPwmFactory::InterruptCount(uint8_t generator, bool clear)
    {
        auto& count = counts[generator];
        return clear ? count.exchange(0) : count.load();
    }

    HilStatus TivaPwmFactory::Parse(uint8_t module, const services::HilArguments& arguments, Settings& requested) const
    {
        requested.module = module;

        HilStatus status = HilStatus::done;
        arguments.Number("freq", requested.frequency, 1, SystemCoreClock, status);
        arguments.Select("mode", requested.centerAligned, alignments, status);
        arguments.Select("div", requested.divisor, divisors, status);
        arguments.Flag("inva", requested.invertA, status);
        arguments.Flag("invb", requested.invertB, status);
        arguments.Select("update", requested.globalUpdate, updateModes, status);
        arguments.Flag("sync", requested.synchronous, status);

        if (status == HilStatus::done && arguments.Has("dead") && arguments.Key("dead") != "off")
        {
            std::array<uint32_t, 2> deadTime{};
            std::size_t count = 0;
            status = ParseNumbers(*arguments.Key("dead"), maximumDeadTimeNs, infra::MakeRange(deadTime), count);
            if (status == HilStatus::done)
                requested.deadTime = DeadTime{ deadTime[0], deadTime[count - 1] };
        }

        if (status == HilStatus::done)
            status = ParseChannels(arguments, requested);
        if (status == HilStatus::done)
            status = ParseSources(arguments, "trigger", requested.channels, &Channel::trigger);
        if (status == HilStatus::done)
            status = ParseSources(arguments, "irq", requested.channels, &Channel::interrupt);
        if (status != HilStatus::done)
            return status;

        if (requested.deadTime && std::max(Cycles(requested.deadTime->rise, requested.divisor), Cycles(requested.deadTime->fall, requested.divisor)) > maximumDeadTimeCycles)
            return HilStatus::range;

        if (!ValidFrequency(requested, requested.frequency))
            return HilStatus::range;

        if (requested.synchronous)
            for (const auto& channel : requested.channels)
                if (channel.interrupt)
                    return HilStatus::unsupported;

        return HilStatus::done;
    }

    HilStatus TivaPwmFactory::ParseChannels(const services::HilArguments& arguments, Settings& requested) const
    {
        auto generators = arguments.Key("gens");
        auto pins = arguments.Key("pins");

        if (!generators && !pins)
            return HilStatus::usage;

        infra::Tokenizer generatorTokens(generators.value_or(infra::BoundedConstString()), ',');
        infra::Tokenizer pinTokens(pins.value_or(infra::BoundedConstString()), ',');
        const auto count = generators ? generatorTokens.Size() : pinTokens.Size();

        if (count == 0 || count > maximumChannels || (generators && pins && generatorTokens.Size() != pinTokens.Size()))
            return HilStatus::usage;

        for (std::size_t i = 0; i != count; ++i)
        {
            Channel channel{};

            if (pins)
            {
                infra::Tokenizer pair(pinTokens.Token(i), ':');
                if (pair.Size() != 2)
                    return HilStatus::usage;

                auto a = ParseOptionalPin(pair.Token(0), naming);
                auto b = ParseOptionalPin(pair.Token(1), naming);
                if (!a || !b)
                    return HilStatus::pin;

                if (!*a && !*b)
                    return HilStatus::usage;

                channel.a = *a;
                channel.b = *b;
            }

            if (generators)
            {
                auto generator = services::HilArguments::ParseNumber(generatorTokens.Token(i));
                if (!generator)
                    return HilStatus::usage;

                if (*generator > maximumGenerator)
                    return HilStatus::range;

                channel.generator = static_cast<uint8_t>(*generator);
            }
            else
            {
                auto pinChannel = ChannelOfPin(channel.a ? *channel.a : *channel.b, requested.module);
                if (!pinChannel)
                    return HilStatus::pin;

                channel.generator = *pinChannel / 2;
            }

            if (!pins)
            {
                channel.a = FindFunctionPin(PwmChannelFunction(2 * channel.generator), requested.module);
                channel.b = FindFunctionPin(PwmChannelFunction(2 * channel.generator + 1), requested.module);
                if (!channel.a && !channel.b)
                    return HilStatus::unsupported;
            }

            for (const auto& other : requested.channels)
                if (other.generator == channel.generator)
                    return HilStatus::usage;

            requested.channels.push_back(channel);
        }

        return HilStatus::done;
    }

    HilStatus TivaPwmFactory::ParseFault(const services::HilArguments& arguments, std::optional<Fault>& fault, std::optional<HilPinId>& pin) const
    {
        bool enable = false;
        uint32_t comparators = 0;
        uint32_t inputs = 0;
        uint32_t minimumPeriod = 0;
        Fault requested;

        HilStatus status = HilStatus::done;
        arguments.SelectAt(1, enable, switches, status);
        arguments.Number("comparators", comparators, 0, maximumComparators, status);
        arguments.Number("inputs", inputs, 0, maximumFaultInputs, status);
        arguments.Number("minperiod", minimumPeriod, 0, maximumFaultPeriod, status);
        arguments.Flag("latch", requested.latch, status);
        arguments.Pin("pin", naming, pin, status);

        for (const auto& channel : settings->channels)
            requested.generators |= 1u << channel.generator;

        if (status == HilStatus::done && arguments.Has("gens"))
        {
            const auto opened = requested.generators;
            std::array<uint32_t, maximumChannels> generators{};
            std::size_t count = 0;
            status = ParseNumbers(*arguments.Key("gens"), maximumGenerator, infra::MakeRange(generators), count);

            requested.generators = 0;
            for (std::size_t i = 0; i != count && status == HilStatus::done; ++i)
            {
                requested.generators |= 1u << generators[i];
                if ((opened & (1u << generators[i])) == 0)
                    status = HilStatus::usage;
            }
        }

        if (status == HilStatus::done && pin && !SupportsFunction(*pin, hal::tiva::PinConfigPeripheral::pwmFault, settings->module))
            status = HilStatus::pin;

        if (status != HilStatus::done)
            return status;

        if (!enable)
        {
            for (auto key : faultKeys)
                if (arguments.Has(key))
                    return HilStatus::usage;

            return HilStatus::done;
        }

        if (comparators == 0 && inputs == 0)
            return HilStatus::usage;

        requested.comparators = static_cast<uint8_t>(comparators);
        requested.inputs = static_cast<uint8_t>(inputs);
        requested.minimumPeriod = static_cast<uint16_t>(minimumPeriod);
        fault = requested;
        return HilStatus::done;
    }

    HilStatus TivaPwmFactory::ClaimPins(services::HilPinOwner& pins, Settings& opened)
    {
        for (auto& channel : opened.channels)
        {
            HilStatus status = pins.ClaimFunction(channel.a, Function(PwmChannelFunction(2 * channel.generator)), opened.module, channel.pinA);
            if (status == HilStatus::done)
                status = pins.ClaimFunction(channel.b, Function(PwmChannelFunction(2 * channel.generator + 1)), opened.module, channel.pinB);

            if (status != HilStatus::done)
                return status;
        }

        return HilStatus::done;
    }

    services::HilPwmHandle& TivaPwmFactory::Construct()
    {
        const auto& opened = *settings;

        auto fillConfig = [this, &opened](auto& config)
        {
            using Config = std::decay_t<decltype(config)>;

            config.channelAInverted = opened.invertA;
            config.channelBInverted = opened.invertB;
            config.control.mode = opened.centerAligned ? Config::Control::Mode::centerAligned : Config::Control::Mode::edgeAligned;
            config.control.updateMode = opened.globalUpdate ? Config::Control::UpdateMode::globally : Config::Control::UpdateMode::locally;
            config.control.debugMode = false;
            config.clockDivisor = static_cast<typename Config::ClockDivisor>(opened.divisor);

            if (opened.deadTime)
                config.deadTime = typename Config::DeadTime{ static_cast<uint16_t>(Cycles(opened.deadTime->fall, opened.divisor)), static_cast<uint16_t>(Cycles(opened.deadTime->rise, opened.divisor)) };
            else
                config.deadTime = std::nullopt;
        };

        auto makeChannels = [&opened](auto& channels)
        {
            using PinChannel = typename std::decay_t<decltype(channels)>::value_type;
            using Trigger = typename PinChannel::Trigger;

            for (const auto& channel : opened.channels)
                channels.push_back(PinChannel{ static_cast<decltype(PinChannel::generator)>(channel.generator), PinOrDummy(channel.pinA), PinOrDummy(channel.pinB),
                    channel.pinA != nullptr, channel.pinB != nullptr, channel.trigger ? std::make_optional(static_cast<Trigger>(infra::enum_cast(*channel.trigger))) : std::nullopt });
        };

        ResetPwmModule(opened.module);

        if (opened.synchronous)
        {
            syncConfig = hal::tiva::SynchronousPwm::Config{};
            fillConfig(syncConfig);

            infra::BoundedVector<hal::tiva::SynchronousPwm::PinChannel>::WithMaxSize<maximumChannels> channels;
            makeChannels(channels);
            return Adapt(driver.emplace<hal::tiva::SynchronousPwm>(opened.module, infra::MakeRange(channels), syncConfig));
        }

        asyncConfig = hal::tiva::Pwm::Config{};
        fillConfig(asyncConfig);

        hal::tiva::Pwm::Config::InterruptConfig interrupts;
        interrupts.priority = opened.fault ? hal::cortex::InterruptPriority::highest : hal::cortex::InterruptPriority::normal;

        for (const auto& channel : opened.channels)
            if (channel.interrupt)
                interrupts.normalSources.push_back({ static_cast<hal::tiva::Pwm::GeneratorIndex>(channel.generator), *channel.interrupt });

        if (opened.fault)
            for (uint8_t generator = 0; generator <= maximumGenerator; ++generator)
                if ((opened.fault->generators & (1u << generator)) != 0)
                    interrupts.faultConfigs.push_back({ static_cast<hal::tiva::Pwm::GeneratorIndex>(generator), opened.fault->inputs, opened.fault->comparators, opened.fault->latch, opened.fault->minimumPeriod });

        if (!interrupts.normalSources.empty() || !interrupts.faultConfigs.empty())
            asyncConfig.interruptConfig = interrupts;

        infra::BoundedVector<hal::tiva::Pwm::PinChannel>::WithMaxSize<maximumChannels> channels;
        makeChannels(channels);
        return Adapt(driver.emplace<hal::tiva::Pwm>(
            opened.module, infra::MakeRange(channels), asyncConfig,
            [this](hal::tiva::Pwm::NormalEvent event)
            {
                counts[infra::enum_cast(event.generator)].fetch_add(1, std::memory_order_relaxed);
            },
            [this](hal::tiva::Pwm::FaultEvent event)
            {
                OnFault(event);
            }));
    }

    template<class Driver>
    services::HilPwmHandle& TivaPwmFactory::Adapt(Driver& pwm)
    {
        auto& handle = adapter.emplace<services::HilPwmAdapter<Driver>>(pwm, settings->channels.size());
        handle.SetBaseFrequency(hal::Hertz(settings->frequency));
        return handle;
    }

    void TivaPwmFactory::ReleaseFaultPin()
    {
        faultPin.reset();
        faultPinId = std::nullopt;
        faultPins.Release();
    }

    void TivaPwmFactory::OnFault(const hal::tiva::Pwm::FaultEvent& event)
    {
        uint8_t comparators = 0;
        uint8_t inputs = 0;
        for (std::size_t i = 0; i != event.comparatorInputsByGenerator.size(); ++i)
        {
            comparators |= infra::enum_cast(event.comparatorInputsByGenerator[i]);
            inputs |= infra::enum_cast(event.inputsByGenerator[i]);
        }

        faultGenerators.fetch_or(infra::enum_cast(event.generatorStatus), std::memory_order_relaxed);
        faultComparators.fetch_or(comparators, std::memory_order_relaxed);
        faultInputs.fetch_or(inputs, std::memory_order_relaxed);
        faultReport.Schedule([this]()
            {
                ReportFault();
            });
    }

    void TivaPwmFactory::ReportFault()
    {
        auto generators = faultGenerators.exchange(0);
        auto comparators = faultComparators.exchange(0);
        auto inputs = faultInputs.exchange(0);

        if (settings)
            response.Event("pwm") << " module=" << settings->module << " gens=" << generators << " comparators=" << comparators << " inputs=" << inputs;
    }

    uint32_t TivaPwmFactory::PwmClock(uint8_t divisor) const
    {
        return SystemCoreClock >> divisor;
    }

    uint32_t TivaPwmFactory::Cycles(uint32_t nanoseconds, uint8_t divisor) const
    {
        return static_cast<uint32_t>(static_cast<uint64_t>(nanoseconds) * PwmClock(divisor) / 1000000000u);
    }

    bool TivaPwmFactory::ValidFrequency(const Settings& opened, uint32_t frequency) const
    {
        const auto period = PwmClock(opened.divisor) / frequency;
        const auto load = opened.centerAligned ? period / 2 : period - 1;
        return period != 0 && load != 0 && load <= maximumLoad;
    }

    PwmExtensionCommands::PwmExtensionCommands(services::HilContext& context, TivaPwmFactory& factory)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , factory(factory)
        , commands{ {
              services::HilBind<PwmExtensionCommands, &PwmExtensionCommands::Fault>("pwm.fault", "<module> <on|off> [gens=] [comparators=] [inputs=] [pin=] [latch=] [minperiod=]", *this, context.response),
              services::HilBind<PwmExtensionCommands, &PwmExtensionCommands::Count>("pwm.count", "<module> <gen> [clear=]", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> PwmExtensionCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    HilStatus PwmExtensionCommands::Fault(const services::HilArguments& arguments)
    {
        if (!arguments.Shape(2, 2, infra::MakeRange(faultKeys)))
            return HilStatus::usage;

        HilStatus status = factory.Find(arguments);
        if (status == HilStatus::done)
            status = factory.ConfigureFault(arguments);
        if (status != HilStatus::done)
            return status;

        context.response.Ok();
        return HilStatus::done;
    }

    HilStatus PwmExtensionCommands::Count(const services::HilArguments& arguments)
    {
        if (!arguments.Shape(2, 2, { "clear" }))
            return HilStatus::usage;

        uint32_t generator = 0;
        bool clear = false;
        HilStatus status = factory.Find(arguments);
        arguments.NumberAt(1, generator, 0, maximumGenerator, status);
        arguments.Flag("clear", clear, status);
        if (status != HilStatus::done)
            return status;

        context.response.Ok() << " count=" << factory.InterruptCount(static_cast<uint8_t>(generator), clear);
        return HilStatus::done;
    }
}
