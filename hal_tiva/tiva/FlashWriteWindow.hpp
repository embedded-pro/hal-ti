#ifndef HAL_TIVA_FLASH_WRITE_WINDOW_HPP
#define HAL_TIVA_FLASH_WRITE_WINDOW_HPP

#include "infra/util/ByteRange.hpp"
#include <array>
#include <cstdint>

namespace hal::tiva
{
    inline constexpr uint32_t flashWriteBufferWords = 32;
    inline constexpr uint32_t flashWriteBufferSize = flashWriteBufferWords * sizeof(uint32_t);

    struct FlashWriteWindow
    {
        uint32_t address;
        std::array<uint32_t, flashWriteBufferWords> words;
        uint32_t wordMask;
    };

    // The controller programs whole words from a 128-byte aligned write buffer; bytes not written stay 0xff,
    // which leaves the flash cell unchanged.
    inline FlashWriteWindow TakeFlashWriteWindow(infra::ConstByteRange& buffer, uint32_t& address)
    {
        FlashWriteWindow window{ address & ~(flashWriteBufferSize - 1), {}, 0 };
        window.words.fill(0xffffffff);

        while (!buffer.empty() && address - window.address < flashWriteBufferSize)
        {
            const auto offset = address - window.address;
            const auto word = offset / sizeof(uint32_t);
            const auto shift = (offset % sizeof(uint32_t)) * 8;

            window.words[word] = (window.words[word] & ~(0xffu << shift)) | (static_cast<uint32_t>(buffer.front()) << shift);
            window.wordMask |= 1u << word;
            buffer.pop_front();
            ++address;
        }

        return window;
    }
}

#endif
