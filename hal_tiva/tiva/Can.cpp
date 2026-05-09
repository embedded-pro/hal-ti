#include "hal_tiva/tiva/Can.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace
{
    extern "C" void Can0_Handler()
    {
        hal::InterruptTable::Instance().Invoke(CAN0_IRQn);
    }

    extern "C" void Can1_Handler()
    {
        hal::InterruptTable::Instance().Invoke(CAN1_IRQn);
    }

    const std::array<CAN0_Type*, 2> peripheralCan = { {
        reinterpret_cast<CAN0_Type*>(CAN0_BASE),
        reinterpret_cast<CAN0_Type*>(CAN1_BASE),
    } };

    constexpr std::array<IRQn_Type, 2> peripheralIrqCan = { {
        CAN0_IRQn,
        CAN1_IRQn,
    } };

    namespace ctl
    {
        inline constexpr uint32_t Init = 1u << 0;
        inline constexpr uint32_t Ie = 1u << 1;
        inline constexpr uint32_t Sie = 1u << 2;
        inline constexpr uint32_t Eie = 1u << 3;
        inline constexpr uint32_t Cce = 1u << 6;
        inline constexpr uint32_t Test = 1u << 7;
        inline constexpr uint32_t AllInterrupts = Ie | Sie | Eie;
    }

    namespace sts
    {
        inline constexpr uint32_t LecMask = 0x07;
        inline constexpr uint32_t TxOk = 1u << 3;
        inline constexpr uint32_t RxOk = 1u << 4;
        inline constexpr uint32_t EPass = 1u << 5;
        inline constexpr uint32_t EWarn = 1u << 6;
        inline constexpr uint32_t BOff = 1u << 7;

        inline constexpr uint32_t LecStuff = 1;
        inline constexpr uint32_t LecForm = 2;
        inline constexpr uint32_t LecAck = 3;
        inline constexpr uint32_t LecBit1 = 4;
        inline constexpr uint32_t LecBit0 = 5;
        inline constexpr uint32_t LecCrc = 6;
    }

    namespace bittiming
    {
        inline constexpr uint32_t BrpMask = 0x3F;
        inline constexpr uint32_t SjwShift = 6;
        inline constexpr uint32_t Tseg1Shift = 8;
        inline constexpr uint32_t Tseg2Shift = 12;
        inline constexpr uint32_t BrpExtMask = 0x0F;
        inline constexpr uint32_t BrpExtShift = 6;
    }

    namespace tst
    {
        inline constexpr uint32_t LBack = 1u << 4;
    }

    namespace interruptStatus
    {
        inline constexpr uint32_t CauseNone = 0x0000;
        inline constexpr uint32_t CauseStatus = 0x8000;
    }

    namespace ifcrq
    {
        inline constexpr uint32_t MnumMask = 0x3F;
        inline constexpr uint32_t Busy = 1u << 15;
    }

    namespace ifcmsk
    {
        inline constexpr uint32_t DataB = 1u << 0;
        inline constexpr uint32_t DataA = 1u << 1;
        inline constexpr uint32_t NewDat = 1u << 2;
        inline constexpr uint32_t ClrIntPnd = 1u << 3;
        inline constexpr uint32_t Control = 1u << 4;
        inline constexpr uint32_t Arb = 1u << 5;
        inline constexpr uint32_t Mask = 1u << 6;
        inline constexpr uint32_t WrNRd = 1u << 7;
    }

    namespace ifarb
    {
        inline constexpr uint32_t Dir = 1u << 13;
        inline constexpr uint32_t Xtd = 1u << 14;
        inline constexpr uint32_t MsgVal = 1u << 15;
        inline constexpr uint32_t Id11Shift = 2;
        inline constexpr uint32_t Id11Mask = 0x7FF;
        inline constexpr uint32_t Id29HighShift = 16;
        inline constexpr uint32_t Id29HighMask = 0x1FFF;
        inline constexpr uint32_t Id29LowMask = 0xFFFF;
        inline constexpr uint32_t Mask11InMsk2 = 0x1FFC;
    }

    namespace ifmctl
    {
        inline constexpr uint32_t DlcMask = 0x0F;
        inline constexpr uint32_t Eob = 1u << 7;
        inline constexpr uint32_t TxRqst = 1u << 8;
        inline constexpr uint32_t RxIe = 1u << 10;
        inline constexpr uint32_t TxIe = 1u << 11;
        inline constexpr uint32_t UMask = 1u << 12;
        inline constexpr uint32_t MsgLst = 1u << 14;
    }

    namespace ifmsk
    {
        inline constexpr uint32_t Mxtd = 1u << 15;
    }

    struct DataWords
    {
        uint32_t da1;
        uint32_t da2;
        uint32_t db1;
        uint32_t db2;
    };

    struct RxReadResult
    {
        hal::tiva::CanRxEntry entry;
        bool messageLost;
    };

    constexpr uint32_t totalMessageObjects = 32;
    constexpr uint8_t maxDataLength = 8;
    constexpr uint8_t rxFilterDlc = 8;

    constexpr uint32_t minTimeQuanta = 8;
    constexpr uint32_t maxTimeQuanta = 25;
    constexpr uint32_t maxTseg1 = 16;
    constexpr uint32_t maxTseg2 = 8;
    constexpr uint32_t maxSjw = 4;
    constexpr uint32_t maxPrescaler = 1024;
    constexpr uint32_t targetSamplePointPermille = 875;

    constexpr uint8_t txMessageObject = 1;
    constexpr uint8_t rxMessageObject = 2;

    std::optional<hal::tiva::Can::Error> LecToError(uint32_t lec)
    {
        switch (lec)
        {
            case sts::LecStuff:
                return hal::tiva::Can::Error::stuffError;
            case sts::LecForm:
                return hal::tiva::Can::Error::formError;
            case sts::LecAck:
                return hal::tiva::Can::Error::ackError;
            case sts::LecBit1:
                return hal::tiva::Can::Error::bit1Error;
            case sts::LecBit0:
                return hal::tiva::Can::Error::bit0Error;
            case sts::LecCrc:
                return hal::tiva::Can::Error::crcError;
            default:
                return std::nullopt;
        }
    }

    void WaitWhileBusy(volatile uint32_t& crq)
    {
        while ((crq & ifcrq::Busy) != 0)
        {
            // spin until controller releases the interface
        }
    }

    DataWords PackMessageData(const hal::Can::Message& data)
    {
        really_assert(data.size() <= maxDataLength);
        const auto byteAt = [&](std::size_t i) -> uint32_t
        {
            return i < data.size() ? data[i] : 0u;
        };
        const auto word = [&](std::size_t lo)
        {
            return byteAt(lo) | (byteAt(lo + 1) << 8);
        };
        return { word(0), word(2), word(4), word(6) };
    }

    void UnpackMessageData(const DataWords& packed, uint8_t dlc, uint8_t* out)
    {
        const uint32_t words[4] = { packed.da1, packed.da2, packed.db1, packed.db2 };
        const uint8_t length = dlc < maxDataLength ? dlc : maxDataLength;
        for (uint8_t i = 0; i < length; ++i)
            out[i] = static_cast<uint8_t>((words[i / 2] >> ((i % 2) * 8)) & 0xFF);
    }

    void WriteArbitration(volatile uint32_t& arb1, volatile uint32_t& arb2, hal::Can::Id id, bool isTx)
    {
        const uint32_t dirFlag = isTx ? ifarb::Dir : 0u;

        if (id.Is29BitId())
        {
            const uint32_t value = id.Get29BitId();
            arb1 = value & ifarb::Id29LowMask;
            arb2 = ifarb::MsgVal | ifarb::Xtd | dirFlag |
                   ((value >> ifarb::Id29HighShift) & ifarb::Id29HighMask);
        }
        else
        {
            arb1 = 0;
            arb2 = ifarb::MsgVal | dirFlag |
                   ((id.Get11BitId() & ifarb::Id11Mask) << ifarb::Id11Shift);
        }
    }

    uint32_t BuildBitReg(uint32_t brp, uint32_t sjw, uint32_t tseg1, uint32_t tseg2)
    {
        using namespace bittiming;
        return (brp & BrpMask) | ((sjw & 0x3) << SjwShift) |
               ((tseg1 & 0xF) << Tseg1Shift) | ((tseg2 & 0x7) << Tseg2Shift);
    }

    void ApplyManualBitTiming(CAN0_Type& can, const hal::tiva::Can::BitTiming& timing)
    {
        really_assert(timing.baudratePrescaler >= 1 && timing.baudratePrescaler <= maxPrescaler);
        really_assert(timing.phaseSegment1 >= 1 && timing.phaseSegment1 <= maxTseg1);
        really_assert(timing.phaseSegment2 >= 1 && timing.phaseSegment2 <= maxTseg2);
        really_assert(timing.synchronizationJumpWidth >= 1 && timing.synchronizationJumpWidth <= maxSjw);

        uint32_t brp = timing.baudratePrescaler - 1u;
        uint32_t sjw = timing.synchronizationJumpWidth - 1u;
        uint32_t tseg1 = timing.phaseSegment1 - 1u;
        uint32_t tseg2 = timing.phaseSegment2 - 1u;

        can.BIT = BuildBitReg(brp, sjw, tseg1, tseg2);
        can.BRPE = (brp >> bittiming::BrpExtShift) & bittiming::BrpExtMask;
    }

    // Computes bit-timing parameters targeting a 87.5% sample point (CiA-301 recommended).
    //
    // The algorithm enumerates all valid time-quanta (tq) counts in the C_CAN range [8, 25]
    // and selects the one whose sample point is closest to 87.5%. Higher tq counts win on tie
    // because they offer better SJW resolution. For the common baudrates the result is:
    //
    //   sysclk   bit-rate  tq  prescaler  phaseSeg1  phaseSeg2  sample-point
    //   ---------------------------------------------------------------------
    //   120 MHz   100 kbit 16     75         13         2          87.50 %
    //   120 MHz   125 kbit 16     60         13         2          87.50 %
    //   120 MHz   250 kbit 16     30         13         2          87.50 %
    //   120 MHz   500 kbit 16     15         13         2          87.50 %
    //   120 MHz     1 Mbit  8     15          6         1          87.50 %
    //    80 MHz   100 kbit 16     50         13         2          87.50 %
    //    80 MHz   125 kbit 16     40         13         2          87.50 %
    //    80 MHz   250 kbit 16     20         13         2          87.50 %
    //    80 MHz   500 kbit 16     10         13         2          87.50 %
    //    80 MHz     1 Mbit 16      5         13         2          87.50 %
    hal::tiva::Can::BitTiming CalculateBitTiming(uint32_t sysclk, uint32_t bitRate)
    {
        really_assert(bitRate > 0);
        really_assert(sysclk % bitRate == 0);

        uint32_t bitClocks = sysclk / bitRate;

        hal::tiva::Can::BitTiming best{};
        uint32_t bestDistance = UINT32_MAX;

        for (uint32_t tq = maxTimeQuanta; tq >= minTimeQuanta; --tq)
        {
            if (bitClocks % tq != 0)
                continue;

            uint32_t prescaler = bitClocks / tq;
            if (prescaler < 1 || prescaler > maxPrescaler)
                continue;

            // round(0.875 * tq) = (1 + phaseSegment1 quanta before sample point)
            uint32_t quantaBeforeSample = (targetSamplePointPermille * tq + 500u) / 1000u;
            if (quantaBeforeSample < 3)
                continue;
            uint32_t phaseSeg1 = quantaBeforeSample - 1u;
            if (phaseSeg1 > maxTseg1)
                continue;
            uint32_t phaseSeg2 = tq - 1u - phaseSeg1;
            if (phaseSeg2 < 1 || phaseSeg2 > maxTseg2)
                continue;

            uint32_t samplePointPermille = quantaBeforeSample * 1000u / tq;
            uint32_t distance = samplePointPermille > targetSamplePointPermille
                                    ? samplePointPermille - targetSamplePointPermille
                                    : targetSamplePointPermille - samplePointPermille;

            if (distance < bestDistance)
            {
                bestDistance = distance;
                best.baudratePrescaler = static_cast<uint16_t>(prescaler);
                best.phaseSegment1 = static_cast<uint8_t>(phaseSeg1);
                best.phaseSegment2 = static_cast<uint8_t>(phaseSeg2);
                best.synchronizationJumpWidth = static_cast<uint8_t>(phaseSeg2 < maxSjw ? phaseSeg2 : maxSjw);
            }
        }

        really_assert(bestDistance != UINT32_MAX);
        return best;
    }

    void EnterInitMode(CAN0_Type& can)
    {
        can.CTL |= ctl::Init;
    }

    void ExitInitMode(CAN0_Type& can)
    {
        can.CTL &= ~ctl::Init;
    }

    void EnableConfigurationAccess(CAN0_Type& can)
    {
        can.CTL |= ctl::Cce;
    }

    void DisableConfigurationAccess(CAN0_Type& can)
    {
        can.CTL &= ~ctl::Cce;
    }

    void EnableInterrupts(CAN0_Type& can)
    {
        can.CTL |= ctl::AllInterrupts;
    }

    void DisableInterrupts(CAN0_Type& can)
    {
        can.CTL &= ~ctl::AllInterrupts;
    }

    void EnablePeripheralClock(uint8_t canIndex)
    {
        SYSCTL->RCGCCAN |= 1u << canIndex;

        while ((SYSCTL->PRCAN & (1u << canIndex)) == 0)
        {
            // spin until peripheral clock is ready
        }
    }

    void DisablePeripheralClock(uint8_t canIndex)
    {
        SYSCTL->RCGCCAN &= ~(1u << canIndex);
    }

    void ConfigureBitTiming(CAN0_Type& can, const std::variant<hal::tiva::Can::BitRate, hal::tiva::Can::BitTiming>& timing)
    {
        std::visit(
            [&can](const auto& selected)
            {
                using T = std::decay_t<decltype(selected)>;
                if constexpr (std::is_same_v<T, hal::tiva::Can::BitTiming>)
                    ApplyManualBitTiming(can, selected);
                else
                    ApplyManualBitTiming(can, CalculateBitTiming(SystemCoreClock, selected));
            },
            timing);
    }

    void ConfigureLoopbackIfRequested(CAN0_Type& can, bool testMode)
    {
        if (!testMode)
            return;

        can.CTL |= ctl::Test;
        can.TST |= tst::LBack;
    }

    void ClearAllMessageObjects(CAN0_Type& can)
    {
        const auto requestAllObjects = [&can]()
        {
            for (uint32_t object = 1; object <= totalMessageObjects; ++object)
            {
                WaitWhileBusy(can.IF1CRQ);
                can.IF1CRQ = object;
            }
        };

        WaitWhileBusy(can.IF1CRQ);
        can.IF1CMSK = ifcmsk::WrNRd | ifcmsk::Arb | ifcmsk::Control;
        can.IF1ARB1 = 0;
        can.IF1ARB2 = 0;
        can.IF1MCTL = 0;
        requestAllObjects();

        WaitWhileBusy(can.IF1CRQ);
        can.IF1CMSK = ifcmsk::NewDat | ifcmsk::ClrIntPnd;
        requestAllObjects();
    }

    void ApplyReceiveFilterForExtendedId(CAN0_Type& can, uint32_t id, uint32_t mask)
    {
        can.IF2MSK1 = mask & ifarb::Id29LowMask;
        can.IF2MSK2 = (mask >> ifarb::Id29HighShift) & ifarb::Id29HighMask;
        can.IF2ARB1 = id & ifarb::Id29LowMask;
        can.IF2ARB2 = ifarb::MsgVal | ifarb::Xtd | ((id >> ifarb::Id29HighShift) & ifarb::Id29HighMask);
    }

    void ApplyReceiveFilterForStandardId(CAN0_Type& can, uint32_t id, uint32_t mask)
    {
        can.IF2MSK1 = 0;
        can.IF2MSK2 = (mask << ifarb::Id11Shift) & ifarb::Mask11InMsk2;
        can.IF2ARB1 = 0;
        can.IF2ARB2 = ifarb::MsgVal | ((id & ifarb::Id11Mask) << ifarb::Id11Shift);
    }

    void ApplyReceiveFilter(CAN0_Type& can, const hal::tiva::Can::Filter& filter)
    {
        if (filter.extended)
            ApplyReceiveFilterForExtendedId(can, filter.id, filter.mask);
        else
            ApplyReceiveFilterForStandardId(can, filter.id, filter.mask);

        if (filter.matchIdType)
            can.IF2MSK2 |= ifmsk::Mxtd;
    }

    void ApplyAcceptAllFilter(CAN0_Type& can)
    {
        can.IF2MSK1 = 0;
        can.IF2MSK2 = ifmsk::Mxtd;
        can.IF2ARB1 = 0;
        can.IF2ARB2 = ifarb::MsgVal;
    }

    void ConfigureReceiveMessageObject(CAN0_Type& can, const std::optional<hal::tiva::Can::Filter>& filter)
    {
        WaitWhileBusy(can.IF2CRQ);
        can.IF2CMSK = ifcmsk::WrNRd | ifcmsk::Arb | ifcmsk::Control | ifcmsk::Mask;

        if (filter)
            ApplyReceiveFilter(can, *filter);
        else
            ApplyAcceptAllFilter(can);

        can.IF2MCTL = ifmctl::RxIe | ifmctl::Eob | ifmctl::UMask | (rxFilterDlc & ifmctl::DlcMask);
        can.IF2CRQ = rxMessageObject;
    }

    void LoadTxMessageObject(CAN0_Type& can, hal::Can::Id id, const hal::Can::Message& data)
    {
        WaitWhileBusy(can.IF1CRQ);

        can.IF1CMSK = ifcmsk::WrNRd | ifcmsk::Arb | ifcmsk::Control | ifcmsk::DataA | ifcmsk::DataB;

        WriteArbitration(can.IF1ARB1, can.IF1ARB2, id, true);

        can.IF1MCTL = ifmctl::TxRqst | ifmctl::TxIe | ifmctl::Eob | (data.size() & ifmctl::DlcMask);

        const auto packed = PackMessageData(data);
        can.IF1DA1 = packed.da1;
        can.IF1DA2 = packed.da2;
        can.IF1DB1 = packed.db1;
        can.IF1DB2 = packed.db2;

        can.IF1CRQ = txMessageObject;
    }

    void ClearMessageObjectInterrupt(CAN0_Type& can, uint32_t objectId)
    {
        WaitWhileBusy(can.IF1CRQ);
        can.IF1CMSK = ifcmsk::ClrIntPnd;
        can.IF1CRQ = objectId & ifcrq::MnumMask;
    }

    RxReadResult ReadRxMessageObject(CAN0_Type& can)
    {
        WaitWhileBusy(can.IF2CRQ);
        can.IF2CMSK = ifcmsk::ClrIntPnd | ifcmsk::NewDat | ifcmsk::Arb | ifcmsk::Control | ifcmsk::DataA | ifcmsk::DataB;
        can.IF2CRQ = rxMessageObject;
        WaitWhileBusy(can.IF2CRQ);

        RxReadResult result{};
        result.messageLost = (can.IF2MCTL & ifmctl::MsgLst) != 0;
        result.entry.length = can.IF2MCTL & ifmctl::DlcMask;
        result.entry.is29Bit = (can.IF2ARB2 & ifarb::Xtd) != 0;
        result.entry.id = result.entry.is29Bit
                              ? ((can.IF2ARB2 & ifarb::Id29HighMask) << ifarb::Id29HighShift) | can.IF2ARB1
                              : (can.IF2ARB2 >> ifarb::Id11Shift) & ifarb::Id11Mask;

        UnpackMessageData({ can.IF2DA1, can.IF2DA2, can.IF2DB1, can.IF2DB2 }, result.entry.length, result.entry.data);

        return result;
    }
}

namespace hal::tiva
{
    Can::Can(infra::MemoryRange<CanRxEntry> rxStorage, uint8_t canIndex, GpioPin& rxPin, GpioPin& txPin, const Config& config, const infra::Function<void(Error)>& onError)
        : ImmediateInterruptHandler(peripheralIrqCan[canIndex], [this]()
              {
                  HandleInterrupt();
              })
        , rxQueue(rxStorage, [this]()
              {
                  ProcessRxBuffer();
              })
        , canIndex(canIndex)
        , rxPin(rxPin, PinConfigPeripheral::canRx)
        , txPin(txPin, PinConfigPeripheral::canTx)
        , config(config)
        , onError(onError)
    {
        EnablePeripheralClock(canIndex);

        auto& can = Peripheral();

        EnterInitMode(can);
        EnableConfigurationAccess(can);
        ConfigureBitTiming(can, this->config.timing);
        ConfigureLoopbackIfRequested(can, this->config.testMode);
        DisableConfigurationAccess(can);

        ClearAllMessageObjects(can);
        ConfigureReceiveMessageObject(can, this->config.filter);

        (void)can.STS;

        EnableInterrupts(can);
        ExitInitMode(can);
    }

    Can::~Can()
    {
        auto& can = Peripheral();

        EnterInitMode(can);
        DisableInterrupts(can);

        const auto irq = peripheralIrqCan[canIndex];
        NVIC_DisableIRQ(irq);
        NVIC_ClearPendingIRQ(irq);

        DisablePeripheralClock(canIndex);
    }

    void Can::SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        really_assert(!sending.load(std::memory_order_acquire));
        really_assert(data.size() <= maxDataLength);

        onSendComplete = actionOnCompletion;
        sending.store(true, std::memory_order_release);

        LoadTxMessageObject(Peripheral(), id, data);
    }

    void Can::ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction)
    {
        onReceive = receivedAction;
    }

    CAN0_Type& Can::Peripheral() const
    {
        return *peripheralCan[canIndex];
    }

    void Can::HandleInterrupt()
    {
        auto& can = Peripheral();

        for (;;)
        {
            const uint32_t intStatus = can.INT;

            if (intStatus == interruptStatus::CauseNone)
                return;

            if (intStatus == interruptStatus::CauseStatus)
                HandleStatusInterrupt();
            else if (intStatus == txMessageObject)
                HandleTxInterrupt();
            else if (intStatus == rxMessageObject)
                HandleRxInterrupt();
            else
                ClearMessageObjectInterrupt(can, intStatus);
        }
    }

    void Can::HandleStatusInterrupt()
    {
        auto& can = Peripheral();

        const uint32_t status = can.STS;
        can.STS = (status & ~(sts::TxOk | sts::RxOk)) | sts::LecMask;

        if (auto lecError = LecToError(status & sts::LecMask))
            ScheduleError(*lecError);

        if ((status & sts::EWarn) != 0)
            ScheduleError(Error::errorWarning);

        if ((status & sts::EPass) != 0)
            ScheduleError(Error::errorPassive);

        if ((status & sts::BOff) != 0)
        {
            ScheduleError(Error::busOff);
            NotifySendFailedFromInterrupt();

            if (config.autoBusOffRecovery)
                ExitInitMode(can);
        }
    }

    void Can::HandleTxInterrupt()
    {
        ClearMessageObjectInterrupt(Peripheral(), txMessageObject);

        if (sending.exchange(false, std::memory_order_acq_rel))
        {
            infra::EventDispatcher::Instance().Schedule([this]()
                {
                    if (onSendComplete)
                        onSendComplete(true);
                });
        }
    }

    void Can::HandleRxInterrupt()
    {
        const auto result = ReadRxMessageObject(Peripheral());

        if (result.messageLost)
            ScheduleError(Error::messageLost);

        if (!rxQueue.Full())
            rxQueue.AddFromInterrupt(result.entry);
        else
            ScheduleError(Error::messageLost);
    }

    void Can::ProcessRxBuffer()
    {
        while (!rxQueue.Empty())
        {
            const CanRxEntry entry = rxQueue.Get();
            const Id id = entry.is29Bit ? Id::Create29BitId(entry.id) : Id::Create11BitId(entry.id);

            Message data;
            const uint8_t length = entry.length < maxDataLength ? entry.length : maxDataLength;
            for (uint8_t i = 0; i < length; ++i)
                data.push_back(entry.data[i]);

            if (onReceive)
                onReceive(id, data);
        }
    }

    void Can::ScheduleError(Error error) const
    {
        infra::EventDispatcher::Instance().Schedule([this, error]()
            {
                if (onError)
                    onError(error);
            });
    }

    void Can::NotifySendFailedFromInterrupt()
    {
        if (sending.exchange(false, std::memory_order_acq_rel))
        {
            infra::EventDispatcher::Instance().Schedule([this]()
                {
                    if (onSendComplete)
                        onSendComplete(false);
                });
        }
    }
}
