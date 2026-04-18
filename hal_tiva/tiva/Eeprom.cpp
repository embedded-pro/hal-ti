#include "hal_tiva/tiva/Eeprom.hpp"
#include "hal_tiva/cortex/InterruptCortex.hpp"
#include "infra/util/ReallyAssert.hpp"

extern "C" void Eeprom_Handler()
{
    hal::InterruptTable::Instance().Invoke(FLASH_CTRL_IRQn);
}

namespace
{
    constexpr uint32_t EedoneWorking = 1u << 0;
    constexpr uint32_t EedoneNoPerm = 1u << 4;
    constexpr uint32_t EesuppEretry = 1u << 2;
    constexpr uint32_t EesuppPretry = 1u << 3;
    constexpr uint32_t EeintProgram = 1u << 0;
    constexpr uint32_t EesizeShiftBlocks = 16u;
    constexpr uint32_t BytesPerBlock = 64u;
    constexpr uint32_t WordsPerBlock = 16u;
    constexpr uint32_t BytesPerWord = 4u;
    constexpr uint32_t FlashFcimEeprom = 1u << 2;
    constexpr uint32_t FlashFcmiscEeprom = 1u << 2;
    constexpr uint32_t EedbgmeMassErase = 1u << 0;
    constexpr uint32_t EedbgmeKey = 0xE37Bu << 16;
    const infra::Duration ErasePollInterval = std::chrono::milliseconds(1);

    void ClearEepromInterrupt()
    {
        FLASH_CTRL->FCMISC = FlashFcmiscEeprom;
        __DSB();
    }

    void AssertNoEepromErrors()
    {
        really_assert((EEPROM->EEDONE & EedoneNoPerm) == 0u);
        really_assert((EEPROM->EESUPP & (EesuppPretry | EesuppEretry)) == 0u);
    }

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

        AssertNoEepromErrors();

        return (EEPROM->EESIZE >> EesizeShiftBlocks) & 0xFFFFu;
    }

    Eeprom::Eeprom()
        : ImmediateInterruptHandler(FLASH_CTRL_IRQn, [this]()
              {
                  HandleInterrupt();
              })
    {
        numberOfBlocks = InitAndGetNumberOfBlocks();
        ClearEepromInterrupt();
        FLASH_CTRL->FCIM |= FlashFcimEeprom;
        EEPROM->EEINT = EeintProgram;
    }

    Eeprom::~Eeprom()
    {
        EEPROM->EEINT = 0u;
        FLASH_CTRL->FCIM &= ~FlashFcimEeprom;
        ClearEepromInterrupt();
        NVIC_DisableIRQ(FLASH_CTRL_IRQn);
        DisableClock();
    }

    void Eeprom::DisableClock() const
    {
        SYSCTL->RCGCEEPROM &= ~1u;
    }

    uint32_t Eeprom::ReadWord(uint32_t block, uint32_t wordOffset) const
    {
        EEPROM->EEBLOCK = block;
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
            uint32_t bufferIndex = 0;

            while (bufferIndex < buffer.size())
            {
                uint32_t wordByteAddr = currentByteAddr & ~(BytesPerWord - 1u);
                uint32_t block = wordByteAddr / BytesPerBlock;
                uint32_t wordOffset = (wordByteAddr / BytesPerWord) % WordsPerBlock;
                uint32_t byteInWord = currentByteAddr - wordByteAddr;
                uint32_t wordData = ReadWord(block, wordOffset);
                uint32_t bytesFromWord = BytesPerWord - byteInWord;

                if (bytesFromWord > (buffer.size() - bufferIndex))
                    bytesFromWord = buffer.size() - bufferIndex;

                for (uint32_t i = 0u; i < bytesFromWord; ++i)
                    buffer[bufferIndex + i] = static_cast<uint8_t>((wordData >> ((byteInWord + i) * 8u)) & 0xFFu);

                currentByteAddr += bytesFromWord;
                bufferIndex += bytesFromWord;
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

        currentOperation = Operation::Writing;
        pendingWriteBuffer = buffer;
        pendingByteAddress = address;
        onOperationDone = onDone;

        StartNextWriteWord();
    }

    void Eeprom::StartNextWriteWord()
    {
        uint32_t wordByteAddr = pendingByteAddress & ~(BytesPerWord - 1u);
        uint32_t block = wordByteAddr / BytesPerBlock;
        uint32_t wordOffset = (wordByteAddr / BytesPerWord) % WordsPerBlock;
        uint32_t byteInWord = pendingByteAddress - wordByteAddr;

        auto bytesAvail = pendingWriteBuffer.size();
        uint32_t bytesToWrite = BytesPerWord - byteInWord;

        if (bytesToWrite > bytesAvail)
            bytesToWrite = bytesAvail;

        uint32_t wordToWrite = 0;

        if (byteInWord != 0u || bytesToWrite < BytesPerWord)
            wordToWrite = ReadWord(block, wordOffset);

        for (uint32_t i = 0u; i < bytesToWrite; ++i)
        {
            uint32_t shift = (byteInWord + i) * 8u;
            wordToWrite = (wordToWrite & ~(0xFFu << shift)) | (static_cast<uint32_t>(pendingWriteBuffer[i]) << shift);
        }

        pendingWriteBuffer = infra::ConstByteRange(pendingWriteBuffer.begin() + bytesToWrite, pendingWriteBuffer.end());
        pendingByteAddress = wordByteAddr + BytesPerWord;

        EEPROM->EEBLOCK = block;
        EEPROM->EEOFFSET = wordOffset;
        EEPROM->EERDWRINC = wordToWrite;
    }

    uint32_t Eeprom::Size() const
    {
        return numberOfBlocks * BytesPerBlock;
    }

    void Eeprom::Erase(infra::Function<void()> onDone)
    {
        really_assert(currentOperation == Operation::Idle);
        really_assert(numberOfBlocks != 0);

        currentOperation = Operation::Erasing;
        onOperationDone = onDone;

        {
            FlashControlInterruptGuard flashControlInterruptGuard;
            ClearEepromInterrupt();
            EEPROM->EEDBGME = EedbgmeKey | EedbgmeMassErase;
        }

        erasePollTimer.Start(ErasePollInterval, [this]()
            {
                PollEraseCompletion();
            });
    }

    void Eeprom::PollEraseCompletion()
    {
        {
            FlashControlInterruptGuard flashControlInterruptGuard;

            if ((EEPROM->EEDONE & EedoneWorking) != 0u)
            {
                erasePollTimer.Start(ErasePollInterval, [this]()
                    {
                        PollEraseCompletion();
                    });
                return;
            }

            ClearEepromInterrupt();
            AssertNoEepromErrors();
        }

        currentOperation = Operation::Idle;
        onOperationDone();
    }

    void Eeprom::HandleInterrupt()
    {
        ClearEepromInterrupt();

        if ((EEPROM->EEDONE & EedoneWorking) != 0u)
            return;

        AssertNoEepromErrors();

        if (currentOperation == Operation::Writing)
        {
            if (!pendingWriteBuffer.empty())
            {
                StartNextWriteWord();
                return;
            }

            currentOperation = Operation::Idle;
            onOperationDone();
        }
    }
}
