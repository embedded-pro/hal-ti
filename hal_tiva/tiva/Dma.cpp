#include "hal_tiva/tiva/Dma.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace hal::tiva
{
    namespace
    {
        constexpr uint32_t SYSCTL_PERIPH_UDMA = 0x00000001;
        constexpr uint32_t UDMA_CFG_MASTEN = 0x00000001;
        constexpr uint32_t UDMA_CHCTL_XFERSIZE_M = 0x00003FF0;
        constexpr uint32_t UDMA_CHCTL_XFERMODE_M = 0x00000007;
        constexpr uint32_t UDMA_CHCTL_SRCINC_M = 0x0C000000;
        constexpr uint32_t UDMA_CHCTL_DSTINC_M = 0xC0000000;
        constexpr uint32_t UDMA_CHCTL_DSTSIZE_M = 0x30000000;
        constexpr uint32_t UDMA_CHCTL_SRCSIZE_M = 0x03000000;
        constexpr uint32_t UDMA_CHCTL_ARBSIZE_M = 0x0003C000;
        constexpr uint32_t UDMA_CHCTL_NXTUSEBURST = 0x00000008;
        constexpr uint32_t UDMA_CHMAP0 = 0x400FF510;

        struct Control
        {
            volatile void* sourceEndAddress;
            volatile void* destinationEndAddress;
            volatile uint32_t channelControl;
            volatile uint32_t reserved;
        };

        constexpr std::array<uint32_t, 4> udmaDstInc = { {
            0x00000000,
            0x40000000,
            0x80000000,
            0xc0000000,
        } };

        constexpr std::array<uint32_t, 4> udmaSrcInc = { {
            0x00000000,
            0x04000000,
            0x08000000,
            0x0c000000,
        } };

        constexpr DmaChannel::Attributes allAttributes{ true, true, true, true };

        volatile uint32_t& Reg(uint32_t reg)
        {
            return *reinterpret_cast<volatile uint32_t*>(reg);
        }

        uint32_t ControlSetMask()
        {
            return UDMA_CHCTL_DSTINC_M | UDMA_CHCTL_DSTSIZE_M | UDMA_CHCTL_SRCINC_M | UDMA_CHCTL_SRCSIZE_M | UDMA_CHCTL_ARBSIZE_M | UDMA_CHCTL_NXTUSEBURST;
        }

        volatile void* GoToEndAddress(volatile void* address, DmaChannel::Increment increment, std::size_t size)
        {
            uint32_t bufferAddress = 0;

            if (increment != DmaChannel::Increment::none)
                bufferAddress = (size << static_cast<std::size_t>(increment)) - 1;

            return static_cast<volatile void*>(reinterpret_cast<volatile uint32_t*>(address) + bufferAddress);
        }

        void Enable()
        {
            UDMA->CFG = UDMA_CFG_MASTEN;
        }

        void Disable()
        {
            UDMA->CFG &= ~UDMA_CFG_MASTEN;
        }

        bool ErrorStatusGet()
        {
            return UDMA->ERRCLR;
        }

        void ErrorStatusClear()
        {
            UDMA->ERRCLR = 1;
        }

        void ChannelEnable(uint8_t channelNumber)
        {
            really_assert(channelNumber < 32);
            auto mask = 1 << channelNumber;
            UDMA->ENASET = mask;
        }

        void ChannelDisable(uint8_t channelNumber)
        {
            really_assert(channelNumber < 32);
            auto mask = 1 << channelNumber;
            UDMA->ENACLR = mask;
        }

        bool IsChannelEnabled(uint8_t channelNumber)
        {
            really_assert(channelNumber < 32);
            auto mask = 1 << channelNumber;
            return (UDMA->ENASET & mask) != 0;
        }

        void SetControlBase(const uint32_t& address)
        {
            really_assert((address & ~0x3FF) == address);
            really_assert(address >= 0x2000000);

            UDMA->CTLBASE = address;
        }

        void ChannelRequest(uint8_t channelNumber)
        {
            really_assert(channelNumber < 32);
            auto mask = 1 << channelNumber;
            UDMA->SWREQ = mask;
        }

        void ChannelAttributeEnable(uint8_t channelNumber, DmaChannel::Attributes attributes)
        {
            really_assert(channelNumber < 32);
            auto mask = 1 << channelNumber;

            if (attributes.useBurst)
                UDMA->USEBURSTSET |= mask;

            if (attributes.alternate)
                UDMA->ALTSET |= mask;

            if (attributes.highPriority)
                UDMA->PRIOSET |= mask;

            if (attributes.requestMask)
                UDMA->REQMASKSET |= mask;
        }

        void ChannelAttributeDisable(uint8_t channelNumber, DmaChannel::Attributes attributes)
        {
            really_assert(channelNumber < 32);
            auto mask = 1 << channelNumber;

            if (attributes.useBurst)
                UDMA->USEBURSTCLR |= mask;

            if (attributes.alternate)
                UDMA->ALTCLR |= mask;

            if (attributes.highPriority)
                UDMA->PRIOCLR |= mask;

            if (attributes.requestMask)
                UDMA->REQMASKCLR |= mask;
        }

        void ChannelControlSet(uint8_t channelNumber, const DmaChannel::ControlBlock& control)
        {
            really_assert(channelNumber < 32);
            auto controlArray = reinterpret_cast<Control*>(UDMA->CTLBASE);
            controlArray[channelNumber].channelControl = (controlArray[channelNumber].channelControl & ~(ControlSetMask())) | control.Read();
        }

        void ChannelSetTransfer(uint8_t channelNumber, DmaChannel::Transfer transfer, volatile void* sourceAddress, volatile void* destinationAddress, std::size_t size)
        {
            really_assert(size > 0 && size <= 1024);
            really_assert(channelNumber < 32);

            auto index = channelNumber * sizeof(Control) / sizeof(uint32_t);
            auto controlArray = reinterpret_cast<Control*>(UDMA->CTLBASE);
            auto control = &controlArray[index];

            auto localControl = (control->channelControl & ~(UDMA_CHCTL_XFERSIZE_M | UDMA_CHCTL_XFERMODE_M));
            localControl |= (size - 1) << 4;
            localControl |= static_cast<uint32_t>(transfer);

            control->sourceEndAddress = GoToEndAddress(sourceAddress, static_cast<DmaChannel::Increment>((localControl & UDMA_CHCTL_SRCINC_M) >> 26), size);
            control->destinationEndAddress = GoToEndAddress(destinationAddress, static_cast<DmaChannel::Increment>((localControl & UDMA_CHCTL_DSTINC_M) >> 30), size);
            control->channelControl = localControl;
        }

        void ChannelAssignMapping(uint8_t channelNumber, uint8_t mapping)
        {
            really_assert(channelNumber < 32);
            really_assert(mapping < 16);

            auto mapRegister = UDMA_CHMAP0 + static_cast<uint32_t>((channelNumber / 8) * 4);
            auto mapShift = (channelNumber % 8) * 4;

            Reg(mapRegister) = (Reg(mapRegister) & ~(0xf << mapShift)) | mapping << mapShift;
        }
    }

    std::array<uint8_t, 1024> Dma::controlTable alignas(1024);

    uint32_t DmaChannel::ControlBlock::Read() const
    {
        return (udmaDstInc[static_cast<uint32_t>(destinationIncrement)]) |
               (udmaSrcInc[static_cast<uint32_t>(sourceIncrement)]) |
               (static_cast<uint32_t>(dataSize)) |
               (static_cast<uint32_t>(arbitrationSize));
    }

    Dma::Dma(const infra::Function<void()>& onError)
        : onError(onError)
    {
        EnableClock();
        Enable();
        SetControlBase(reinterpret_cast<uint32_t>(Dma::controlTable.data()));

        Register(UDMAERR_IRQn);
    }

    Dma::~Dma()
    {
        Disable();
        DisableClock();
    }

    bool Dma::IsEnabled() const
    {
        return (UDMA->CFG & UDMA_CFG_MASTEN) != 0;
    }

    void Dma::Invoke()
    {
        if (ErrorStatusGet())
        {
            ErrorStatusClear();
            onError();
        }
    }

    void Dma::EnableClock() const
    {
        SYSCTL->RCGCDMA |= SYSCTL_PERIPH_UDMA;

        while (!(SYSCTL->PRDMA & SYSCTL_PERIPH_UDMA))
        {
            // Wait for the DMA module to be ready
        }
    }

    void Dma::DisableClock() const
    {
        SYSCTL->RCGCDMA &= ~SYSCTL_PERIPH_UDMA;
    }

    DmaChannel::DmaChannel(Dma& dma, const Channel& channel, const Configuration& configuration)
        : channel(channel)
    {
        really_assert(dma.IsEnabled());

        ChannelAttributeDisable(channel.number, allAttributes);
        ChannelAssignMapping(channel.number, channel.mapping);
        ChannelControlSet(channel.number, configuration.controlBlock);
        ChannelAttributeEnable(channel.number, configuration.attributes);
    }

    DmaChannel::~DmaChannel()
    {
        if (IsChannelEnabled(channel.number))
            ChannelDisable(channel.number);

        ChannelAttributeDisable(channel.number, allAttributes);
    }

    void DmaChannel::StartTransfer(Transfer transfer, volatile void* sourceAddress, volatile void* destinationAddress, std::size_t size) const
    {
        ChannelSetTransfer(channel.number, transfer, sourceAddress, destinationAddress, size);
        ChannelEnable(channel.number);
        // ChannelRequest(channel.number);
    }

    std::size_t DmaChannel::MaxTransferSize() const
    {
        constexpr static std::size_t maxTransferSize = 1024;

        return maxTransferSize;
    }
}
