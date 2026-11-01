#ifndef HAL_TIVA_FLASH_INTERNAL_HPP
#define HAL_TIVA_FLASH_INTERNAL_HPP

#include "hal/interfaces/FlashHomogeneous.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"

namespace hal::tiva
{
    class FlashInternal
        : public hal::FlashHomogeneous
    {
    public:
        explicit FlashInternal(infra::ConstByteRange flashMemory);

        void WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
        void ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
        void EraseSectors(uint32_t beginIndex, uint32_t endIndex, infra::Function<void()> onDone) override;

    private:
        uint32_t AbsoluteAddress(uint32_t address) const;

        infra::ConstByteRange flashMemory;
    };
}

#endif
