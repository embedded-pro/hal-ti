#ifndef HAL_TIVA_SYNCHRONOUS_FLASH_INTERNAL_HPP
#define HAL_TIVA_SYNCHRONOUS_FLASH_INTERNAL_HPP

#include "hal/synchronous_interfaces/SynchronousFlashHomogeneous.hpp"
#include "infra/util/ByteRange.hpp"

namespace hal::tiva
{
    class SynchronousFlashInternal
        : public hal::SynchronousFlashHomogeneous
    {
    public:
        explicit SynchronousFlashInternal(infra::ConstByteRange flashMemory);

        void WriteBuffer(infra::ConstByteRange buffer, uint32_t address) override;
        void ReadBuffer(infra::ByteRange buffer, uint32_t address) override;
        void EraseSectors(uint32_t beginIndex, uint32_t endIndex) override;

    private:
        uint32_t AbsoluteAddress(uint32_t address) const;

        infra::ConstByteRange flashMemory;
    };
}

#endif
