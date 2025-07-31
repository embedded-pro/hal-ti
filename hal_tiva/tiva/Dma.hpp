#ifndef HAL_DMA_STM_HPP
#define HAL_DMA_STM_HPP

#include "infra/util/InterfaceConnector.hpp"
#include DEVICE_HEADER
#include "hal_tiva/cortex/InterruptCortex.hpp"

namespace hal::tiva
{
    class Dma
        : public infra::InterfaceConnector<Dma>
        , private InterruptHandler
    {
    public:
        explicit Dma(const infra::Function<void()>& onError);
        virtual ~Dma();

        bool IsEnabled() const;

        // Implementation of InterruptHandler
        void Invoke() override;

    private:
        void EnableClock() const;
        void DisableClock() const;

        infra::Function<void()> onError;
        static std::array<uint8_t, 1024> controlTable alignas(1024);
    };

    class DmaChannel
    {
    public:
        enum class Transfer : uint32_t
        {
            stop = 0x00,
            basic = 0x01,
            automatic = 0x02,
            pingPong = 0x03,
        };

        enum class DataSize : uint32_t
        {
            _8_bits = 0x00000000,
            _16_bits = 0x11000000,
            _32_bits = 0x22000000,
        };

        enum class Increment : uint8_t
        {
            _8_bits,
            _16_bits,
            _32_bits,
            none,
        };

        enum class ArbitrationSize : uint32_t
        {
            _1_item = 0x00000000,
            _2_items = 0x00004000,
            _4_items = 0x00008000,
            _8_items = 0x0000c000,
            _16_items = 0x00010000,
            _32_items = 0x00014000,
            _64_items = 0x00018000,
            _128_items = 0x0001c000,
            _256_items = 0x00020000,
            _512_items = 0x00024000,
            _1024_items = 0x00028000,
        };

        struct Attributes
        {
            bool useBurst;
            bool alternate;
            bool highPriority;
            bool requestMask;
        };

        struct ControlBlock
        {
            Increment sourceIncrement;
            Increment destinationIncrement;
            DataSize dataSize;
            ArbitrationSize arbitrationSize;

            uint32_t Read() const;
        };

        struct Configuration
        {
            Attributes attributes;
            ControlBlock controlBlock;
        };

        struct Channel
        {
            uint8_t number;
            uint8_t mapping;
        };

        struct Buffers
        {
            volatile void* sourceAddress;
            volatile void* destinationAddress;
            std::size_t size;
        };

        DmaChannel(Dma& dma, const Channel& channel, const Configuration& configuration);
        ~DmaChannel();

        void StartTransfer(Transfer transfer, const Buffers& buffer) const;
        void StartPingPongTransfer(const Buffers& primaryBuffer, const Buffers& alternateBuffer) const;
        bool IsPrimaryTransferCompleted() const;
        bool IsAlternateTransferCompleted() const;
        void StopTransfer() const;
        void ForceRequest() const;
        std::size_t MaxTransferSize() const;

    private:
        const Channel& channel;
    };
}

#endif
