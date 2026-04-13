#pragma once

#include "hal/interfaces/Eeprom.hpp"
#include "hal_tiva/cortex/InterruptCortex.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>

namespace hal::tiva
{
    class Eeprom
        : public hal::Eeprom
        , private hal::ImmediateInterruptHandler
    {
    public:
        Eeprom();
        ~Eeprom();

        uint32_t Size() const override;
        void WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
        void ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
        void Erase(infra::Function<void()> onDone) override;

    private:
        static uint32_t InitAndGetNumberOfBlocks();
        void DisableClock() const;
        void HandleInterrupt();
        void StartNextWriteWord();
        void StartNextErase() const;
        uint32_t ReadWord(uint32_t block, uint32_t wordOffset) const;

    private:
        enum class Operation : uint8_t
        {
            Idle,
            Writing,
            Erasing,
        };

        Operation currentOperation = Operation::Idle;

        infra::ConstByteRange pendingWriteBuffer;
        uint32_t pendingByteAddress = 0;

        uint32_t eraseCurrentBlock = 0;
        uint32_t numberOfBlocks = 0;

        infra::Function<void()> onOperationDone;
    };
}
