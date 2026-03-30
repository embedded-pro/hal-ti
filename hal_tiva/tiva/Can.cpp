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

    constexpr std::array<IRQn_Type, 2> peripheralIrqCanArray = { {
        CAN0_IRQn,
        CAN1_IRQn,
    } };

    constexpr uint32_t CAN_CTL_INIT = 1 << 0;
    constexpr uint32_t CAN_CTL_IE = 1 << 1;
    constexpr uint32_t CAN_CTL_SIE = 1 << 2;
    constexpr uint32_t CAN_CTL_EIE = 1 << 3;
    constexpr uint32_t CAN_CTL_DAR = 1 << 5;
    constexpr uint32_t CAN_CTL_CCE = 1 << 6;
    constexpr uint32_t CAN_CTL_TEST = 1 << 7;

    constexpr uint32_t CAN_STS_LEC_M = 0x07;
    constexpr uint32_t CAN_STS_TXOK = 1 << 3;
    constexpr uint32_t CAN_STS_RXOK = 1 << 4;
    constexpr uint32_t CAN_STS_EPASS = 1 << 5;
    constexpr uint32_t CAN_STS_EWARN = 1 << 6;
    constexpr uint32_t CAN_STS_BOFF = 1 << 7;

    constexpr uint32_t CAN_STS_LEC_STUFF = 1;
    constexpr uint32_t CAN_STS_LEC_FORM = 2;
    constexpr uint32_t CAN_STS_LEC_ACK = 3;
    constexpr uint32_t CAN_STS_LEC_BIT1 = 4;
    constexpr uint32_t CAN_STS_LEC_BIT0 = 5;
    constexpr uint32_t CAN_STS_LEC_CRC = 6;

    constexpr uint32_t CAN_BIT_BRP_M = 0x3F;
    constexpr uint32_t CAN_BIT_SJW_S = 6;
    constexpr uint32_t CAN_BIT_SJW_M = 0x3 << CAN_BIT_SJW_S;
    constexpr uint32_t CAN_BIT_TSEG1_S = 8;
    constexpr uint32_t CAN_BIT_TSEG1_M = 0xF << CAN_BIT_TSEG1_S;
    constexpr uint32_t CAN_BIT_TSEG2_S = 12;
    constexpr uint32_t CAN_BIT_TSEG2_M = 0x7 << CAN_BIT_TSEG2_S;

    constexpr uint32_t CAN_TST_LBACK = 1 << 4;

    constexpr uint32_t CAN_INT_STS_CAUSE_STATUS = 0x8000;

    constexpr uint32_t CAN_IFCRQ_BUSY = 1 << 15;

    constexpr uint32_t CAN_IFCMSK_DATAB = 1 << 0;
    constexpr uint32_t CAN_IFCMSK_DATAA = 1 << 1;
    constexpr uint32_t CAN_IFCMSK_NEWDAT = 1 << 2;
    constexpr uint32_t CAN_IFCMSK_TXRQST = 1 << 2;
    constexpr uint32_t CAN_IFCMSK_CLRINTPND = 1 << 3;
    constexpr uint32_t CAN_IFCMSK_CONTROL = 1 << 4;
    constexpr uint32_t CAN_IFCMSK_ARB = 1 << 5;
    constexpr uint32_t CAN_IFCMSK_MASK = 1 << 6;
    constexpr uint32_t CAN_IFCMSK_WRNRD = 1 << 7;

    constexpr uint32_t CAN_IFARB2_DIR = 1 << 13;
    constexpr uint32_t CAN_IFARB2_XTD = 1 << 14;
    constexpr uint32_t CAN_IFARB2_MSGVAL = 1 << 15;

    constexpr uint32_t CAN_IFMCTL_DLC_M = 0x0F;
    constexpr uint32_t CAN_IFMCTL_EOB = 1 << 7;
    constexpr uint32_t CAN_IFMCTL_TXRQST = 1 << 8;
    constexpr uint32_t CAN_IFMCTL_RMTEN = 1 << 9;
    constexpr uint32_t CAN_IFMCTL_RXIE = 1 << 10;
    constexpr uint32_t CAN_IFMCTL_TXIE = 1 << 11;
    constexpr uint32_t CAN_IFMCTL_UMASK = 1 << 12;
    constexpr uint32_t CAN_IFMCTL_INTPND = 1 << 13;
    constexpr uint32_t CAN_IFMCTL_MSGLST = 1 << 14;
    constexpr uint32_t CAN_IFMCTL_NEWDAT = 1 << 15;

    constexpr uint32_t CAN_IFMSK2_MXTD = 1 << 15;
    constexpr uint32_t CAN_IFMSK2_MDIR = 1 << 14;

    std::optional<hal::tiva::Can::Error> LecToError(uint32_t lec)
    {
        switch (lec)
        {
            case CAN_STS_LEC_STUFF:
                return hal::tiva::Can::Error::stuffError;
            case CAN_STS_LEC_FORM:
                return hal::tiva::Can::Error::formError;
            case CAN_STS_LEC_ACK:
                return hal::tiva::Can::Error::ackError;
            case CAN_STS_LEC_BIT1:
                return hal::tiva::Can::Error::bit1Error;
            case CAN_STS_LEC_BIT0:
                return hal::tiva::Can::Error::bit0Error;
            case CAN_STS_LEC_CRC:
                return hal::tiva::Can::Error::crcError;
            default:
                return std::nullopt;
        }
    }

    void WaitIfBusy(volatile uint32_t& crq)
    {
        while (crq & CAN_IFCRQ_BUSY)
        {
            // do nothing until not busy
        }
    }

    void SetArbitration11Bit(CAN0_Type& can, uint32_t id, bool isTx)
    {
        can.IF1ARB1 = 0;
        can.IF1ARB2 = CAN_IFARB2_MSGVAL | (isTx ? CAN_IFARB2_DIR : 0) | ((id & 0x7FF) << 2);
    }

    void SetArbitration29Bit(CAN0_Type& can, uint32_t id, bool isTx)
    {
        can.IF1ARB1 = id & 0xFFFF;
        can.IF1ARB2 = CAN_IFARB2_MSGVAL | CAN_IFARB2_XTD | (isTx ? CAN_IFARB2_DIR : 0) | ((id >> 16) & 0x1FFF);
    }

    void WriteData(CAN0_Type& can, const hal::Can::Message& data)
    {
        uint32_t da1 = 0;
        uint32_t da2 = 0;
        uint32_t db1 = 0;
        uint32_t db2 = 0;

        if (data.size() > 0)
            da1 |= data[0];
        if (data.size() > 1)
            da1 |= static_cast<uint32_t>(data[1]) << 8;
        if (data.size() > 2)
            da2 |= data[2];
        if (data.size() > 3)
            da2 |= static_cast<uint32_t>(data[3]) << 8;
        if (data.size() > 4)
            db1 |= data[4];
        if (data.size() > 5)
            db1 |= static_cast<uint32_t>(data[5]) << 8;
        if (data.size() > 6)
            db2 |= data[6];
        if (data.size() > 7)
            db2 |= static_cast<uint32_t>(data[7]) << 8;

        can.IF1DA1 = da1;
        can.IF1DA2 = da2;
        can.IF1DB1 = db1;
        can.IF1DB2 = db2;
    }

    void ReadData(const CAN0_Type& can, hal::Can::Message& data, uint8_t dlc)
    {
        data.clear();

        if (dlc > 0)
            data.push_back(static_cast<uint8_t>(can.IF2DA1 & 0xFF));
        if (dlc > 1)
            data.push_back(static_cast<uint8_t>((can.IF2DA1 >> 8) & 0xFF));
        if (dlc > 2)
            data.push_back(static_cast<uint8_t>(can.IF2DA2 & 0xFF));
        if (dlc > 3)
            data.push_back(static_cast<uint8_t>((can.IF2DA2 >> 8) & 0xFF));
        if (dlc > 4)
            data.push_back(static_cast<uint8_t>(can.IF2DB1 & 0xFF));
        if (dlc > 5)
            data.push_back(static_cast<uint8_t>((can.IF2DB1 >> 8) & 0xFF));
        if (dlc > 6)
            data.push_back(static_cast<uint8_t>(can.IF2DB2 & 0xFF));
        if (dlc > 7)
            data.push_back(static_cast<uint8_t>((can.IF2DB2 >> 8) & 0xFF));
    }

    hal::Can::Id ReadArbitration(const CAN0_Type& can)
    {
        if (can.IF2ARB2 & CAN_IFARB2_XTD)
        {
            uint32_t id = ((can.IF2ARB2 & 0x1FFF) << 16) | can.IF2ARB1;
            return hal::Can::Id::Create29BitId(id);
        }
        else
        {
            uint32_t id = (can.IF2ARB2 >> 2) & 0x7FF;
            return hal::Can::Id::Create11BitId(id);
        }
    }

    uint32_t BuildBitReg(uint32_t brp, uint32_t sjw, uint32_t tseg1, uint32_t tseg2)
    {
        return (brp & CAN_BIT_BRP_M) | ((sjw & 0x3) << CAN_BIT_SJW_S) | ((tseg1 & 0xF) << CAN_BIT_TSEG1_S) | ((tseg2 & 0x7) << CAN_BIT_TSEG2_S);
    }

    void ApplyManualBitTiming(CAN0_Type& can, const hal::tiva::Can::BitTiming& timing)
    {
        uint32_t brp = timing.baudratePrescaler - 1;
        uint32_t sjw = timing.synchronizationJumpWidth - 1;
        uint32_t tseg1 = timing.phaseSegment1 - 1;
        uint32_t tseg2 = timing.phaseSegment2 - 1;

        can.BIT = BuildBitReg(brp, sjw, tseg1, tseg2);
        can.BRPE = (brp >> 6) & 0x0F;
    }

    constexpr uint32_t minTimeQuanta = 8;
    constexpr uint32_t maxTimeQuanta = 25;
    constexpr uint32_t maxTseg1 = 16;
    constexpr uint32_t maxTseg2 = 8;
    constexpr uint32_t maxSjw = 4;
    constexpr uint32_t maxPrescaler = 1024;

    uint32_t ClampedTseg2(uint32_t tq)
    {
        uint32_t tseg2 = (tq - 1) / 3;
        if (tseg2 > maxTseg2)
            return maxTseg2;
        if (tseg2 < 1)
            return 1;
        return tseg2;
    }

    bool IsValidSegmentation(uint32_t tq, uint32_t tseg1, uint32_t tseg2)
    {
        return tseg1 >= 1 && tseg1 <= maxTseg1 && (1 + tseg1 + tseg2) == tq;
    }

    void ApplyBitTimingRegisters(CAN0_Type& can, uint32_t prescaler, uint32_t sjw, uint32_t tseg1, uint32_t tseg2)
    {
        can.BIT = BuildBitReg(prescaler - 1, sjw - 1, tseg1 - 1, tseg2 - 1);
        can.BRPE = ((prescaler - 1) >> 6) & 0x0F;
    }

    void CalculateAndApplyBitTiming(CAN0_Type& can, uint32_t bitRate)
    {
        uint32_t bitClocks = SystemCoreClock / bitRate;

        for (uint32_t prescaler = 1; prescaler <= maxPrescaler; ++prescaler)
        {
            uint32_t tq = bitClocks / prescaler;

            if (tq < minTimeQuanta || tq > maxTimeQuanta)
                continue;

            uint32_t tseg2 = ClampedTseg2(tq);
            uint32_t tseg1 = tq - 1 - tseg2;

            if (!IsValidSegmentation(tq, tseg1, tseg2))
                continue;

            uint32_t sjw = tseg2 > maxSjw ? maxSjw : tseg2;
            ApplyBitTimingRegisters(can, prescaler, sjw, tseg1, tseg2);
            return;
        }

        really_assert(false);
    }
}

namespace hal::tiva
{
    Can::Can(infra::BoundedDeque<std::pair<Id, Message>>& rxBuffer, uint8_t canIndex, GpioPin& high, GpioPin& low, const Config& config, const infra::Function<void(Error)>& onError)
        : ImmediateInterruptHandler(peripheralIrqCanArray[canIndex], [this]()
              {
                  HandleInterrupt();
              })
        , rxBuffer(rxBuffer)
        , canIndex(canIndex)
        , high(high, PinConfigPeripheral::canRx)
        , low(low, PinConfigPeripheral::canTx)
        , config(config)
        , onError(onError)
    {
        EnableClock();

        auto& can = *peripheralCan[canIndex];

        can.CTL = CAN_CTL_INIT;
        can.CTL |= CAN_CTL_CCE;

        ConfigureBitTiming();

        if (config.testMode)
        {
            can.CTL |= CAN_CTL_TEST;
            can.TST |= CAN_TST_LBACK;
        }

        can.CTL |= CAN_CTL_IE | CAN_CTL_SIE | CAN_CTL_EIE;
        can.CTL &= ~(CAN_CTL_INIT | CAN_CTL_CCE);

        ConfigureReceiveMessageObject();
    }

    Can::~Can()
    {
        auto& can = *peripheralCan[canIndex];

        can.CTL |= CAN_CTL_INIT;
        can.CTL &= ~(CAN_CTL_IE | CAN_CTL_SIE | CAN_CTL_EIE);

        DisableClock();
    }

    void Can::SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        really_assert(!sending);

        auto& can = *peripheralCan[canIndex];

        sending = true;
        onSendComplete = actionOnCompletion;

        WaitIfBusy(can.IF1CRQ);

        can.IF1CMSK = CAN_IFCMSK_WRNRD | CAN_IFCMSK_ARB | CAN_IFCMSK_CONTROL | CAN_IFCMSK_DATAA | CAN_IFCMSK_DATAB;

        if (id.Is29BitId())
            SetArbitration29Bit(can, id.Get29BitId(), true);
        else
            SetArbitration11Bit(can, id.Get11BitId(), true);

        can.IF1MCTL = CAN_IFMCTL_TXRQST | CAN_IFMCTL_TXIE | CAN_IFMCTL_EOB | (data.size() & CAN_IFMCTL_DLC_M);

        WriteData(can, data);

        can.IF1CRQ = txMessageObject;
    }

    void Can::ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction)
    {
        onReceive = receivedAction;
    }

    void Can::HandleInterrupt()
    {
        auto& can = *peripheralCan[canIndex];

        uint32_t intStatus = can.INT;

        if (intStatus == CAN_INT_STS_CAUSE_STATUS)
            HandleStatusInterrupt(can);
        else if (intStatus == txMessageObject)
            HandleTxInterrupt(can);
        else if (intStatus == rxMessageObject)
            HandleRxInterrupt(can);
    }

    void Can::ScheduleError(Error error) const
    {
        infra::EventDispatcher::Instance().Schedule([this, error]()
            {
                if (onError)
                    onError(error);
            });
    }

    void Can::HandleStatusInterrupt(const CAN0_Type& can) const
    {
        uint32_t status = can.STS;

        if (status & CAN_STS_BOFF)
        {
            ScheduleError(Error::busOff);
            return;
        }

        if (status & CAN_STS_EPASS)
        {
            ScheduleError(Error::errorPassive);
            return;
        }

        if (status & CAN_STS_EWARN)
        {
            ScheduleError(Error::errorWarning);
            return;
        }

        auto error = LecToError(status & CAN_STS_LEC_M);
        if (error)
            ScheduleError(*error);
    }

    void Can::HandleTxInterrupt(CAN0_Type& can)
    {
        WaitIfBusy(can.IF1CRQ);
        can.IF1CMSK = CAN_IFCMSK_CLRINTPND;
        can.IF1CRQ = txMessageObject;

        if (sending)
        {
            sending = false;
            infra::EventDispatcher::Instance().Schedule([this]()
                {
                    if (onSendComplete)
                        onSendComplete(true);
                });
        }
    }

    void Can::HandleRxInterrupt(CAN0_Type& can)
    {
        WaitIfBusy(can.IF2CRQ);
        can.IF2CMSK = CAN_IFCMSK_CLRINTPND | CAN_IFCMSK_NEWDAT | CAN_IFCMSK_ARB | CAN_IFCMSK_CONTROL | CAN_IFCMSK_DATAA | CAN_IFCMSK_DATAB;
        can.IF2CRQ = rxMessageObject;
        WaitIfBusy(can.IF2CRQ);

        if (can.IF2MCTL & CAN_IFMCTL_MSGLST)
            ScheduleError(Error::messageLost);

        uint8_t dlc = can.IF2MCTL & CAN_IFMCTL_DLC_M;

        Message data;
        ReadData(can, data, dlc);

        Id receivedId = ReadArbitration(can);

        if (!rxBuffer.full())
            rxBuffer.push_back(std::make_pair(receivedId, data));
        else
            ScheduleError(Error::messageLost);

        infra::EventDispatcher::Instance().Schedule([this]()
            {
                ProcessRxBuffer();
            });
    }

    void Can::ProcessRxBuffer()
    {
        while (!rxBuffer.empty())
        {
            auto [id, data] = rxBuffer.front();
            rxBuffer.pop_front();

            if (onReceive)
                onReceive(id, data);
        }
    }

    void Can::ConfigureBitTiming() const
    {
        auto& can = *peripheralCan[canIndex];

        if (config.bitTiming)
            ApplyManualBitTiming(can, *config.bitTiming);
        else
            CalculateAndApplyBitTiming(can, config.bitRate);
    }

    void Can::ConfigureReceiveMessageObject() const
    {
        auto& can = *peripheralCan[canIndex];

        WaitIfBusy(can.IF2CRQ);

        can.IF2CMSK = CAN_IFCMSK_WRNRD | CAN_IFCMSK_ARB | CAN_IFCMSK_CONTROL | CAN_IFCMSK_MASK;

        can.IF2MSK1 = 0;
        can.IF2MSK2 = 0;

        can.IF2ARB1 = 0;
        can.IF2ARB2 = CAN_IFARB2_MSGVAL;

        can.IF2MCTL = CAN_IFMCTL_RXIE | CAN_IFMCTL_EOB | CAN_IFMCTL_UMASK | (8 & CAN_IFMCTL_DLC_M);

        can.IF2CRQ = rxMessageObject;
    }

    void Can::EnableClock() const
    {
        SYSCTL->RCGCCAN |= 1 << canIndex;

        while ((SYSCTL->PRCAN & (1 << canIndex)) == 0)
        {
            // do nothing until peripheral is ready
        }
    }

    void Can::DisableClock() const
    {
        SYSCTL->RCGCCAN &= ~(1 << canIndex);
    }
}
