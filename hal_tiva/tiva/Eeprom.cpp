#include "hal_tiva/tiva/Eeprom.hpp"
#include "hal_tiva/cortex/InterruptCortex.hpp"
#include "infra/util/ReallyAssert.hpp"

extern "C" void Eeprom_Handler()
{
    hal::InterruptTable::Instance().Invoke(FLASH_CTRL_IRQn);
}

namespace
{
    constexpr uint32_t EedoneWorking     = 1u << 0;
    constexpr uint32_t EesuppPretry      = 1u << 1;
    constexpr uint32_t EesuppEretry      = 1u << 2;
    constexpr uint32_t EesuppEraseCmd    = 0x02u;
    constexpr uint32_t EeintDone         = 1u << 2;
    constexpr uint32_t EesizeShiftBlocks = 16u;
    constexpr uint32_t BytesPerBlock     = 16u;
    constexpr uint32_t WordsPerBlock     = 4u;
    constexpr uint32_t BytesPerWord      = 4u;

    class FlashControlInterruptGuard
    {
    public:
        FlashControlInterruptGuard()
        {
            NVIC_DisableIRQ(FLASH_CTRL_IRQn);
        }

        ~FlashControlInterruptGuard()
        {
            NVIC_EnableIRQ(FLASH_CTRL_IRQn);
        }
    };
}

namespace hal::tiva
{
    uint32_t Eeprom::InitAndGetNumberOfBlocks()
    {
        SYSCTL->RCGCEEPROM |= 1u;
        while ((SYSCTL->PREEPROM & 1u) == 0)
        {
            // Wait for peripheral clock to be ready
        }

        while ((EEPROM->EEDONE & EedoneWorking) != 0)
        {
            // Wait for EEPROM to be idle
        }

        really_assert((EEPROM->EESUPP & (EesuppPretry | EesuppEretry)) == 0);

        SYSCTL->SREEPROM |= 1u;
        SYSCTL->SREEPROM &= ~1u;

        while ((SYSCTL->PREEPROM & 1u) == 0)
        {
            // Wait for peripheral clock to be ready after reset
        }

        while ((EEPROM->EEDONE & EedoneWorking) != 0)
        {
            // Wait for EEPROM to be idle after reset
        }

        return (EEPROM->EESIZE >> EesizeShiftBlocks) & 0xFFFFu;
    }

    Eeprom::Eeprom()
        : ImmediateInterruptHandler(FLASH_CTRL_IRQn, [this]()
              {
                  HandleInterrupt();
              })
    {
        numberOfBlocks = InitAndGetNumberOfBlocks();
        EEPROM->EEINT  = EeintDone;
    }

    Eeprom::~Eeprom()
    {
        EEPROM->EEINT = 0u;
        NVIC_DisableIRQ(FLASH_CTRL_IRQn);
        DisableClock();
    }

    void Eeprom::DisableClock() const
    {
        SYSCTL->RCGCEEPROM &= ~1u;
    }

    uint32_t Eeprom::ReadWord(uint32_t block, uint32_t wordOffset) const
    {
        EEPROM->EEBLOCK  = block;
        EEPROM->EEOFFSET = wordOffset;
        return EEPROM->EERDWR;
    }

    void Eeprom::ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        const uint32_t totalSize = Size();

        really_assert(currentOperation == Operation::Idle);
        really_assert(address <= totalSize);
        really_assert(buffer.size() <= totalSize - address);

        {
            FlashControlInterruptGuard flashControlInterruptGuard;

            uint32_t currentByteAddr = address;
            uint32_t bufferIndex     = 0;

            while (bufferIndex < buffer.size())
            {
                uint32_t wordByteAddr   = currentByteAddr & ~(BytesPerWord - 1u);
                uint32_t block          = wordByteAddr / BytesPerBlock;
                uint32_t wordOffset     = (wordByteAddr / BytesPerWord) % WordsPerBlock;
                uint32_t byteInWord     = currentByteAddr - wordByteAddr;

                uint32_t wordData = ReadWord(block, wordOffset);

                uint32_t bytesFromWord = BytesPerWord - byteInWord;
                if (bytesFromWord > (buffer.size() - bufferIndex))
                    bytesFromWord = buffer.size() - bufferIndex;

                for (uint32_t i = 0u; i < bytesFromWord; ++i)
                {
                    buffer[bufferIndex + i] = static_cast<uint8_t>((wordData >> ((byteInWord + i) * 8u)) & 0xFFu);
                }

                currentByteAddr += bytesFromWord;
                bufferIndex     += bytesFromWord;
            }
        }

        onDone();
    }

    void Eeprom::WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        const uint32_t totalSize = Size();
        FlashControlInterruptGuard flashControlInterruptGuard;

        really_assert(currentOperation == Operation::Idle);
        really_assert(!buffer.empty());
        really_assert(address <= totalSize);
        really_assert(buffer.size() <= totalSize - address);

        currentOperation   = Operation::Writing;
        pendingWriteBuffer = buffer;
        pendingByteAddress = address;
        onOperationDone    = onDone;

        StartNextWriteWord();
    }

    void Eeprom::StartNextWriteWord()
    {
        uint32_t wordByteAddr = pendingByteAddress & ~(BytesPerWord - 1u);
        uint32_t block        = wordByteAddr / BytesPerBlock;
        uint32_t wordOffset   = (wordByteAddr / BytesPerWord) % WordsPerBlock;
        uint32_t byteInWord   = pendingByteAddress - wordByteAddr;

        auto bytesAvail       = pendingWriteBuffer.size();
        uint32_t bytesToWrite = BytesPerWord - byteInWord;
        if (bytesToWrite > bytesAvail)
            bytesToWrite = bytesAvail;

        uint32_t wordToWrite;
        if (byteInWord != 0u || bytesToWrite < BytesPerWord)
        {
            wordToWrite = ReadWord(block, wordOffset);
        }
        else
        {
            wordToWrite = 0u;
        }

        for (uint32_t i = 0u; i < bytesToWrite; ++i)
        {
            uint32_t shift = (byteInWord + i) * 8u;
            wordToWrite    = (wordToWrite & ~(0xFFu << shift)) | (static_cast<uint32_t>(pendingWriteBuffer[i]) << shift);
        }

        pendingWriteBuffer = infra::ConstByteRange(pendingWriteBuffer.begin() + bytesToWrite, pendingWriteBuffer.end());
        pendingByteAddress = wordByteAddr + BytesPerWord;

        EEPROM->EEBLOCK   = block;
        EEPROM->EEOFFSET  = wordOffset;
        EEPROM->EERDWRINC = wordToWrite;
    }

    uint32_t Eeprom::Size() const
    {
        return numberOfBlocks * BytesPerBlock;
    }

    void Eeprom::Erase(infra::Function<void()> onDone)
    {
        FlashControlInterruptGuard flashControlInterruptGuard;

        really_assert(currentOperation == Operation::Idle);
        really_assert(numberOfBlocks != 0);

        currentOperation  = Operation::Erasing;
        eraseCurrentBlock = 0;
        onOperationDone   = onDone;

        StartNextErase();
    }

    void Eeprom::StartNextErase() const
    {
        EEPROM->EEBLOCK = eraseCurrentBlock;
        EEPROM->EESUPP  = EesuppEraseCmd;
    }

    void Eeprom::HandleInterrupt()
    {
        if ((EEPROM->EEDONE & EedoneWorking) != 0u)
            return;

        if (currentOperation == Operation::Writing)
        {
            if (!pendingWriteBuffer.empty())
            {
                StartNextWriteWord();
                return;
            }

            currentOperation = Operation::Idle;
            infra::Function<void()> done = onOperationDone;
            onOperationDone              = infra::Function<void()>{};
            done();
        }
        else if (currentOperation == Operation::Erasing)
        {
            ++eraseCurrentBlock;
            if (eraseCurrentBlock < numberOfBlocks)
            {
                StartNextErase();
                return;
            }

            currentOperation = Operation::Idle;
            infra::Function<void()> done = onOperationDone;
            onOperationDone              = infra::Function<void()>{};
            done();
        }
    }
}
