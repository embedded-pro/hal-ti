#include "hal_tiva/tiva/FlashController.hpp"
#include DEVICE_HEADER
#include "FlashFamily.hpp"
#include "hal_tiva/tiva/FlashWriteWindow.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace
{
    constexpr uint32_t FmcErase = 1u << 1;
    constexpr uint32_t Fmc2WriteBuffer = 1u << 0;
    constexpr uint32_t BootcfgKey = 1u << 4;
    constexpr uint32_t WriteKeyA442 = 0xA442u << 16;
    constexpr uint32_t WriteKey71D5 = 0x71D5u << 16;
    constexpr uint32_t FcrisAccess = 1u << 0;
    constexpr uint32_t FcrisVoltage = 1u << 9;
    constexpr uint32_t FcrisInvalidData = 1u << 10;
    constexpr uint32_t FcrisEraseVerify = 1u << 11;
    constexpr uint32_t FcrisProgramVerify = 1u << 13;
    constexpr uint32_t EraseErrors = FcrisAccess | FcrisVoltage | FcrisEraseVerify;
    constexpr uint32_t ProgramErrors = FcrisAccess | FcrisVoltage | FcrisInvalidData | FcrisProgramVerify;

    uint32_t WriteKey()
    {
        return (FLASH_CTRL->BOOTCFG & BootcfgKey) != 0 ? WriteKeyA442 : WriteKey71D5;
    }

    void ProgramWindow(const hal::tiva::FlashWriteWindow& window)
    {
        volatile uint32_t* writeBuffer = &FLASH_CTRL->FWBN;

        FLASH_CTRL->FMA = window.address;
        for (uint32_t word = 0; word != hal::tiva::flashWriteBufferWords; ++word)
            if ((window.wordMask & (1u << word)) != 0)
                writeBuffer[word] = window.words[word];

        FLASH_CTRL->FMC2 = WriteKey() | Fmc2WriteBuffer;
        while ((FLASH_CTRL->FMC2 & Fmc2WriteBuffer) != 0)
        {
            // Wait for the write buffer to be programmed
        }
    }
}

namespace hal::tiva
{
    infra::ConstByteRange SectorAlignedRegion(infra::ConstByteRange flashMemory)
    {
        really_assert(reinterpret_cast<uintptr_t>(flashMemory.begin()) % family::flashSectorSize == 0);
        really_assert(flashMemory.size() % family::flashSectorSize == 0);
        return flashMemory;
    }

    void EraseFlashSector(uint32_t address)
    {
        FLASH_CTRL->FCMISC = EraseErrors;
        FLASH_CTRL->FMA = address;
        FLASH_CTRL->FMC = WriteKey() | FmcErase;
        while ((FLASH_CTRL->FMC & FmcErase) != 0)
        {
            // Wait for the sector to be erased
        }

        really_assert((FLASH_CTRL->FCRIS & EraseErrors) == 0);
    }

    void ProgramFlash(infra::ConstByteRange buffer, uint32_t address)
    {
        FLASH_CTRL->FCMISC = ProgramErrors;
        while (!buffer.empty())
            ProgramWindow(TakeFlashWriteWindow(buffer, address));

        really_assert((FLASH_CTRL->FCRIS & ProgramErrors) == 0);
    }
}
