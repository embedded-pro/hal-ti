#include "hal_tiva/tiva/Can.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

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

    constexpr uint32_t CanIfarb1IdM = 0x0000FFFF;

    constexpr uint32_t CanIfarb2Msgval = 0x00008000;
    constexpr uint32_t CanIfarb2Xtd = 0x00004000;
    constexpr uint32_t CanIfarb2Dir = 0x00002000;
    constexpr uint32_t CanIfarb2IdM = 0x00001FFF;

    constexpr uint32_t CanIfcmskWrnrd = 0x00000080;
    constexpr uint32_t CanIfcmskMask = 0x00000040;
    constexpr uint32_t CanIfcmskArb = 0x00000020;
    constexpr uint32_t CanIfcmskControl = 0x00000010;
    constexpr uint32_t CanIfcmskClrintpnd = 0x00000008;

    constexpr uint32_t CanIfcmskNewdat = 0x00000004;
    constexpr uint32_t CanIfcmskTxrqst = 0x00000004;
    constexpr uint32_t CanIfcmskDataa = 0x00000002;
    constexpr uint32_t CanIfcmskDatab = 0x00000001;

    constexpr uint32_t CanIfmsk1IdmskM = 0x0000FFFF;

    constexpr uint32_t CanIfmsk2Mxtd = 0x00008000;
    constexpr uint32_t CanIfmsk2Mdir = 0x00004000;
    constexpr uint32_t CanIfmsk2IdmskM = 0x00001FFF;

    constexpr uint32_t CanCtlInit = 1u << 0;
    constexpr uint32_t CanCtlIe = 1u << 1;
    constexpr uint32_t CanCtlSie = 1u << 2;
    constexpr uint32_t CanCtlEie = 1u << 3;
    constexpr uint32_t CanCtlCce = 1u << 6;
    constexpr uint32_t CanCtlTest = 1u << 7;

    constexpr uint32_t CanStsLecM = 0x07u;
    constexpr uint32_t CanStsTxok = 1u << 3;
    constexpr uint32_t CanStsRxok = 1u << 4;
    constexpr uint32_t CanStsEpass = 1u << 5;
    constexpr uint32_t CanStsEwarn = 1u << 6;
    constexpr uint32_t CanStsBoff = 1u << 7;

    constexpr uint32_t CanStsLecStuff = 1;
    constexpr uint32_t CanStsLecForm = 2;
    constexpr uint32_t CanStsLecAck = 3;
    constexpr uint32_t CanStsLecBit1 = 4;
    constexpr uint32_t CanStsLecBit0 = 5;
    constexpr uint32_t CanStsLecCrc = 6;

    constexpr uint32_t CanBitBrpM = 0x3Fu;
    constexpr uint32_t CanBitSjwS = 6u;
    constexpr uint32_t CanBitSjwM = 0x3u << CanBitSjwS;
    constexpr uint32_t CanBitTseg1S = 8u;
    constexpr uint32_t CanBitTseg1M = 0xFu << CanBitTseg1S;
    constexpr uint32_t CanBitTseg2S = 12u;
    constexpr uint32_t CanBitTseg2M = 0x7u << CanBitTseg2S;

    constexpr uint32_t CanBrpeBrpeM = 0x0000000Fu;

    constexpr uint32_t CanTstLback = 1u << 4;

    constexpr uint32_t CanIntStsCauseStatus = 0x8000u;

    constexpr uint32_t CanIfcrqBusy = 1u << 15;

    constexpr uint32_t CanIfmctlNewdat = 0x00008000u;
    constexpr uint32_t CanIfmctlMsglst = 0x00004000u;
    constexpr uint32_t CanIfmctlIntpnd = 0x00002000u;
    constexpr uint32_t CanIfmctlUmask = 0x00001000u;
    constexpr uint32_t CanIfmctlTxie = 0x00000800u;
    constexpr uint32_t CanIfmctlRxie = 0x00000400u;
    constexpr uint32_t CanIfmctlRmten = 0x00000200u;
    constexpr uint32_t CanIfmctlTxrqst = 0x00000100u;
    constexpr uint32_t CanIfmctlEob = 0x00000080u;
    constexpr uint32_t CanIfmctlDlcM = 0x0000000Fu;

    constexpr uint32_t CanMaxBitDivisor = 19u;
    constexpr uint32_t CanMinBitDivisor = 4u;
    constexpr uint32_t CanMaxPreDivisor = 1024u;
    constexpr uint32_t CanMinPreDivisor = 1u;

    constexpr uint32_t CanIf1crqMnumM = 0x3Fu;

    constexpr uint16_t BitValue(uint32_t seg1, uint32_t seg2, uint32_t sjw)
    {
        return static_cast<uint16_t>(
            (((seg1 - 1) << CanBitTseg1S) & CanBitTseg1M) |
            (((seg2 - 1) << CanBitTseg2S) & CanBitTseg2M) |
            (((sjw - 1) << CanBitSjwS) & CanBitSjwM));
    }

    // clang-format off
    constexpr std::array<uint16_t, 16> bitValuesLookupTable =
    { {
        BitValue(2,  1, 1),  // 4  clocks/bit
        BitValue(3,  1, 1),  // 5  clocks/bit
        BitValue(3,  2, 2),  // 6  clocks/bit
        BitValue(4,  2, 2),  // 7  clocks/bit
        BitValue(4,  3, 3),  // 8  clocks/bit
        BitValue(5,  3, 3),  // 9  clocks/bit
        BitValue(5,  4, 4),  // 10 clocks/bit
        BitValue(6,  4, 4),  // 11 clocks/bit
        BitValue(6,  5, 4),  // 12 clocks/bit
        BitValue(7,  5, 4),  // 13 clocks/bit
        BitValue(7,  6, 4),  // 14 clocks/bit
        BitValue(8,  6, 4),  // 15 clocks/bit
        BitValue(8,  7, 4),  // 16 clocks/bit
        BitValue(9,  7, 4),  // 17 clocks/bit
        BitValue(9,  8, 4),  // 18 clocks/bit
        BitValue(10, 8, 4),  // 19 clocks/bit
    } };
    // clang-format on

    // -------------------------------------------------------------------------
    // Error decode
    // -------------------------------------------------------------------------

    std::optional<hal::tiva::Can::Error> LecToError(uint32_t lec)
    {
        switch (lec)
        {
            case CanStsLecStuff:
                return hal::tiva::Can::Error::stuffError;
            case CanStsLecForm:
                return hal::tiva::Can::Error::formError;
            case CanStsLecAck:
                return hal::tiva::Can::Error::ackError;
            case CanStsLecBit1:
                return hal::tiva::Can::Error::bit1Error;
            case CanStsLecBit0:
                return hal::tiva::Can::Error::bit0Error;
            case CanStsLecCrc:
                return hal::tiva::Can::Error::crcError;
            default:
                return std::nullopt;
        }
    }

    struct ArbitrationFields
    {
        uint32_t arb1;
        uint32_t arb2;
    };

    ArbitrationFields EncodeArbitration(hal::Can::Id id, bool isTx)
    {
        uint32_t arb1;
        uint32_t arb2 = CanIfarb2Msgval | (isTx ? CanIfarb2Dir : 0u);

        if (id.Is29BitId())
        {
            arb1 = id.Get29BitId() & CanIfarb1IdM;
            arb2 |= ((id.Get29BitId() >> 16) & CanIfarb2IdM) | CanIfarb2Xtd;
        }
        else
        {
            arb1 = 0;
            arb2 |= (id.Get11BitId() << 2) & CanIfarb2IdM;
        }

        return { arb1, arb2 };
    }

    struct MaskFields
    {
        uint32_t msk1;
        uint32_t msk2;
    };

    MaskFields EncodeMask(uint32_t maskId, bool is29Bit, bool dirFilter)
    {
        uint32_t msk1;
        uint32_t msk2;

        if (is29Bit)
        {
            msk1 = maskId & CanIfmsk1IdmskM;
            msk2 = ((maskId >> 16) & CanIfmsk2IdmskM) | CanIfmsk2Mxtd;
        }
        else
        {
            msk1 = 0;
            msk2 = ((maskId << 2) & CanIfmsk2IdmskM);
        }

        if (dirFilter)
            msk2 |= CanIfmsk2Mdir;

        return { msk1, msk2 };
    }

    struct If1Regs
    {
        static volatile uint32_t& Crq(CAN0_Type& c)
        {
            return c.IF1CRQ;
        }

        static volatile uint32_t& Cmsk(CAN0_Type& c)
        {
            return c.IF1CMSK;
        }

        static volatile uint32_t& Msk1(CAN0_Type& c)
        {
            return c.IF1MSK1;
        }

        static volatile uint32_t& Msk2(CAN0_Type& c)
        {
            return c.IF1MSK2;
        }

        static volatile uint32_t& Arb1(CAN0_Type& c)
        {
            return c.IF1ARB1;
        }

        static volatile uint32_t& Arb2(CAN0_Type& c)
        {
            return c.IF1ARB2;
        }

        static volatile uint32_t& Mctl(CAN0_Type& c)
        {
            return c.IF1MCTL;
        }

        static volatile uint32_t& Da1(CAN0_Type& c)
        {
            return c.IF1DA1;
        }

        static volatile uint32_t& Da2(CAN0_Type& c)
        {
            return c.IF1DA2;
        }

        static volatile uint32_t& Db1(CAN0_Type& c)
        {
            return c.IF1DB1;
        }

        static volatile uint32_t& Db2(CAN0_Type& c)
        {
            return c.IF1DB2;
        }
    };

    struct If2Regs
    {
        static volatile uint32_t& Crq(CAN0_Type& c)
        {
            return c.IF2CRQ;
        }

        static volatile uint32_t& Cmsk(CAN0_Type& c)
        {
            return c.IF2CMSK;
        }

        static volatile uint32_t& Msk1(CAN0_Type& c)
        {
            return c.IF2MSK1;
        }

        static volatile uint32_t& Msk2(CAN0_Type& c)
        {
            return c.IF2MSK2;
        }

        static volatile uint32_t& Arb1(CAN0_Type& c)
        {
            return c.IF2ARB1;
        }

        static volatile uint32_t& Arb2(CAN0_Type& c)
        {
            return c.IF2ARB2;
        }

        static volatile uint32_t& Mctl(CAN0_Type& c)
        {
            return c.IF2MCTL;
        }

        static volatile uint32_t& Da1(CAN0_Type& c)
        {
            return c.IF2DA1;
        }

        static volatile uint32_t& Da2(CAN0_Type& c)
        {
            return c.IF2DA2;
        }

        static volatile uint32_t& Db1(CAN0_Type& c)
        {
            return c.IF2DB1;
        }

        static volatile uint32_t& Db2(CAN0_Type& c)
        {
            return c.IF2DB2;
        }
    };

    template<typename IF>
    class MessageInterface
    {
    public:
        explicit MessageInterface(CAN0_Type& can)
            : can(can)
        {
            WaitForIdle();
        }

        void SetCommand(uint32_t cmask)
        {
            IF::Cmsk(can) = cmask;
        }

        void SetArbitration(ArbitrationFields arb)
        {
            IF::Arb1(can) = arb.arb1;
            IF::Arb2(can) = arb.arb2;
        }

        void SetMask(MaskFields msk)
        {
            IF::Msk1(can) = msk.msk1;
            IF::Msk2(can) = msk.msk2;
        }

        void SetControl(uint32_t mctl)
        {
            IF::Mctl(can) = mctl;
        }

        void Commit(uint8_t msgNum)
        {
            IF::Crq(can) = msgNum & CanIf1crqMnumM;
        }

        void WaitForIdle() const
        {
            while (IF::Crq(can) & CanIfcrqBusy)
            {
            }
        }

        uint32_t ReadArb1() const
        {
            return IF::Arb1(can);
        }

        uint32_t ReadArb2() const
        {
            return IF::Arb2(can);
        }

        uint32_t ReadMsk1() const
        {
            return IF::Msk1(can);
        }

        uint32_t ReadMsk2() const
        {
            return IF::Msk2(can);
        }

        uint32_t ReadControl() const
        {
            return IF::Mctl(can);
        }

        void WriteData(const hal::Can::Message& data)
        {
            auto* reg = reinterpret_cast<volatile uint32_t*>(&IF::Da1(can));

            for (std::size_t i = 0; i < data.size();)
            {
                auto value = static_cast<uint32_t>(data[i++]);

                if (i < data.size())
                    value |= static_cast<uint32_t>(data[i++]) << 8;

                *reg++ = value;
            }
        }

        void ReadData(uint8_t* dst, uint8_t count)
        {
            auto* reg = reinterpret_cast<volatile uint32_t*>(&IF::Da1(can));

            for (uint8_t i = 0; i < count;)
            {
                uint32_t var = *reg++;
                dst[i++] = static_cast<uint8_t>(var & 0xFFu);

                if (i < count)
                    dst[i++] = static_cast<uint8_t>(var >> 8u);
            }
        }

    private:
        CAN0_Type& can;
    };

    extern "C" uint32_t SystemCoreClock;

    void WriteBitTimingRaw(CAN0_Type& can, uint32_t brpMinus1, uint32_t sjwMinus1, uint32_t tseg1Minus1, uint32_t tseg2Minus1)
    {
        can.BIT = (brpMinus1 & CanBitBrpM) | ((sjwMinus1 & 0x3u) << CanBitSjwS) | ((tseg1Minus1 & 0xFu) << CanBitTseg1S) | ((tseg2Minus1 & 0x7u) << CanBitTseg2S);
        can.BRPE = (brpMinus1 >> 6) & CanBrpeBrpeM;
    }

    void ApplyManualBitTiming(CAN0_Type& can, const hal::tiva::Can::BitTiming& timing)
    {
        really_assert(timing.baudratePrescaler >= 1);
        really_assert(timing.phaseSegment1 >= 1);
        really_assert(timing.phaseSegment2 >= 1);
        really_assert(timing.synchronizationJumpWidth >= 1);

        uint32_t savedCtl = can.CTL;
        can.CTL = savedCtl | CanCtlInit | CanCtlCce;

        WriteBitTimingRaw(can,
            timing.baudratePrescaler - 1,
            timing.synchronizationJumpWidth - 1,
            timing.phaseSegment1 - 1,
            timing.phaseSegment2 - 1);

        can.CTL = savedCtl & ~CanCtlCce;
    }

    void CalculateAndApplyBitTiming(CAN0_Type& can, uint32_t bitRate)
    {
        uint32_t bitClocks = SystemCoreClock / bitRate;

        really_assert(bitClocks <= (CanMaxPreDivisor * CanMaxBitDivisor));
        really_assert(bitClocks >= (CanMinPreDivisor * CanMinBitDivisor));

        if ((SystemCoreClock / bitClocks) > bitRate)
            ++bitClocks;

        while (bitClocks <= (CanMaxPreDivisor * CanMaxBitDivisor))
        {
            for (uint32_t bits = CanMaxBitDivisor; bits >= CanMinBitDivisor; --bits)
            {
                uint32_t preDivisor = bitClocks / bits;

                if ((preDivisor * bits) == bitClocks)
                {
                    uint32_t savedCtl = can.CTL;
                    can.CTL = savedCtl | CanCtlInit | CanCtlCce;

                    can.BIT = ((preDivisor - 1) & CanBitBrpM) | bitValuesLookupTable[bits - CanMinBitDivisor];
                    can.BRPE = ((preDivisor - 1) >> 6) & CanBrpeBrpeM;

                    can.CTL = savedCtl & ~CanCtlCce;
                    return;
                }
            }

            ++bitClocks;
        }

        really_assert(false);
    }
}

namespace hal::tiva
{
    Can::Can(infra::MemoryRange<CanRxEntry> rxStorage, uint8_t canIndex, GpioPin& rxPin, GpioPin& txPin, const Config& config, const infra::Function<void(Error)>& onError)
        : ImmediateInterruptHandler(peripheralIrqCanArray[canIndex], [this]()
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
        EnableClock();
        Initialization();
        ConfigureBitTiming();

        if (config.filter)
            ConfigureFilter();
        else
            ConfigureReceiveMessageObject();

        EnableInterrupts();
        Enable();
    }

    Can::~Can()
    {
        DisableInterrupts();
        DisableClock();
    }

    void Can::SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        if (sending.exchange(true))
        {
            if (actionOnCompletion)
                actionOnCompletion(false);
            return;
        }

        onSendComplete = actionOnCompletion;

        auto& can = *peripheralCan[canIndex];
        MessageInterface<If1Regs> mi{ can };
        mi.SetCommand(CanIfcmskWrnrd | CanIfcmskDataa | CanIfcmskDatab | CanIfcmskControl | CanIfcmskArb);
        mi.WriteData(data);
        mi.SetArbitration(EncodeArbitration(id, true));
        mi.SetControl(CanIfmctlTxrqst | (static_cast<uint32_t>(data.size()) & CanIfmctlDlcM) | CanIfmctlTxie | CanIfmctlEob);
        mi.Commit(txMessageObject);
    }

    void Can::ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction)
    {
        onReceive = receivedAction;
    }

    void Can::HandleInterrupt()
    {
        auto& can = *peripheralCan[canIndex];

        uint32_t cause = can.INT;

        if (cause == CanIntStsCauseStatus)
            HandleStatusInterrupt(can);
        else if (cause == txMessageObject)
            HandleTxInterrupt(can);
        else if (cause == rxMessageObject)
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

    void Can::HandleStatusInterrupt(CAN0_Type& can)
    {
        uint32_t status = can.STS;
        can.STS = ~(CanStsRxok | CanStsTxok | CanStsLecM);

        StatusLevel currentLevel = StatusLevel::ok;
        if (status & CanStsBoff)
            currentLevel = StatusLevel::busOff;
        else if (status & CanStsEpass)
            currentLevel = StatusLevel::passive;
        else if (status & CanStsEwarn)
            currentLevel = StatusLevel::warning;

        if (currentLevel > previousStatusLevel)
        {
            switch (currentLevel)
            {
                case StatusLevel::busOff:
                    ScheduleError(Error::busOff);
                    break;
                case StatusLevel::passive:
                    ScheduleError(Error::errorPassive);
                    break;
                case StatusLevel::warning:
                    ScheduleError(Error::errorWarning);
                    break;
                case StatusLevel::ok:
                    break;
            }
        }

        previousStatusLevel = currentLevel;

        auto error = LecToError(status & CanStsLecM);
        if (error)
            ScheduleError(*error);
    }

    void Can::HandleTxInterrupt(CAN0_Type& can)
    {
        MessageInterface<If1Regs> mi{ can };
        mi.SetCommand(CanIfcmskClrintpnd);
        mi.Commit(txMessageObject);
        mi.WaitForIdle();

        if (sending.exchange(false))
            infra::EventDispatcher::Instance().Schedule([this]()
                {
                    if (onSendComplete)
                        onSendComplete(true);
                });
    }

    void Can::HandleRxInterrupt(CAN0_Type& can)
    {
        CanRxEntry entry{};

        MessageInterface<If2Regs> mi{ can };
        mi.SetCommand(CanIfcmskDataa | CanIfcmskDatab | CanIfcmskControl | CanIfcmskMask | CanIfcmskArb | CanIfcmskClrintpnd);
        mi.Commit(rxMessageObject);
        mi.WaitForIdle();

        uint32_t arbReg1 = mi.ReadArb2();
        uint32_t arbReg0 = mi.ReadArb1();
        uint32_t msgCtrl = mi.ReadControl();

        if ((!(msgCtrl & CanIfmctlTxrqst) && (arbReg1 & CanIfarb2Dir)) ||
            ((msgCtrl & CanIfmctlTxrqst) && !(arbReg1 & CanIfarb2Dir)))
            entry.isRemoteFrame = true;

        entry.is29Bit = (arbReg1 & CanIfarb2Xtd) != 0;

        if (entry.is29Bit)
            entry.id = ((arbReg1 & CanIfarb2IdM) << 16) | arbReg0;
        else
            entry.id = (arbReg1 & CanIfarb2IdM) >> 2;

        if (msgCtrl & CanIfmctlMsglst)
            ScheduleError(Error::messageLost);

        if (msgCtrl & CanIfmctlNewdat)
        {
            entry.length = std::min<uint8_t>(static_cast<uint8_t>(msgCtrl & CanIfmctlDlcM), 8u);

            if (!entry.isRemoteFrame)
                mi.ReadData(entry.data, entry.length);

            MessageInterface<If2Regs> mi2{ can };
            mi2.SetCommand(CanIfcmskNewdat);
            mi2.Commit(rxMessageObject);
            mi2.WaitForIdle();

            if (!rxQueue.Full())
                rxQueue.AddFromInterrupt(entry);
            else
                ScheduleError(Error::messageLost);
        }
    }

    void Can::ProcessRxBuffer()
    {
        while (!rxQueue.Empty())
        {
            CanRxEntry entry = rxQueue.Get();

            Id id = entry.is29Bit ? Id::Create29BitId(entry.id) : Id::Create11BitId(entry.id);

            Message data;
            for (uint8_t i = 0; i < entry.length; ++i)
                data.push_back(entry.data[i]);

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

        MessageInterface<If1Regs> mi{ can };
        mi.SetCommand(CanIfcmskWrnrd | CanIfcmskArb | CanIfcmskControl | CanIfcmskMask);
        mi.SetMask(MaskFields{ 0, 0 });
        mi.SetArbitration(ArbitrationFields{ 0, CanIfarb2Msgval });
        mi.SetControl(CanIfmctlRxie | CanIfmctlEob | CanIfmctlUmask | (8u & CanIfmctlDlcM));
        mi.Commit(rxMessageObject);
    }

    void Can::ConfigureFilter() const
    {
        auto& can = *peripheralCan[canIndex];

        auto id = config.filter->is29Bit ? Id::Create29BitId(config.filter->id) : Id::Create11BitId(config.filter->id);

        MessageInterface<If1Regs> mi{ can };
        mi.SetCommand(CanIfcmskWrnrd | CanIfcmskDataa | CanIfcmskDatab | CanIfcmskControl | CanIfcmskMask | CanIfcmskArb);
        mi.SetMask(EncodeMask(config.filter->mask, config.filter->is29Bit, true));
        mi.SetArbitration(EncodeArbitration(id, false));
        mi.SetControl(CanIfmctlUmask | (8u & CanIfmctlDlcM) | CanIfmctlRxie | CanIfmctlEob);
        mi.Commit(rxMessageObject);
    }

    void Can::ClearAllMessageObjects() const
    {
        auto& can = *peripheralCan[canIndex];

        for (uint8_t msgObj = 1; msgObj <= 32; ++msgObj)
        {
            MessageInterface<If1Regs> mi{ can };
            mi.Commit(msgObj);
        }

        for (uint8_t msgObj = 1; msgObj <= 32; ++msgObj)
        {
            MessageInterface<If1Regs> mi{ can };
            mi.SetCommand(CanIfcmskNewdat | CanIfcmskClrintpnd);
            mi.Commit(msgObj);
        }
    }

    void Can::EnableClock() const
    {
        SYSCTL->RCGCCAN |= 1u << canIndex;

        while ((SYSCTL->PRCAN & (1u << canIndex)) == 0)
        {
        }
    }

    void Can::DisableClock() const
    {
        SYSCTL->RCGCCAN &= ~(1u << canIndex);
    }

    void Can::Enable() const
    {
        auto& can = *peripheralCan[canIndex];
        can.CTL &= ~CanCtlInit;
    }

    void Can::Initialization() const
    {
        auto& can = *peripheralCan[canIndex];

        can.CTL = CanCtlInit;

        MessageInterface<If1Regs> mi{ can };
        mi.SetCommand(CanIfcmskWrnrd | CanIfcmskArb | CanIfcmskControl);
        mi.SetArbitration(ArbitrationFields{ 0, 0 });
        mi.SetControl(0);

        ClearAllMessageObjects();

        (void)can.STS;
    }

    void Can::EnableInterrupts() const
    {
        auto& can = *peripheralCan[canIndex];
        can.CTL |= CanCtlIe | CanCtlSie | CanCtlEie;
    }

    void Can::DisableInterrupts() const
    {
        auto& can = *peripheralCan[canIndex];
        can.CTL &= ~(CanCtlIe | CanCtlSie | CanCtlEie);
    }
}
