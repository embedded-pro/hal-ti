#include "hal_tiva/tiva/FlashWriteWindow.hpp"
#include "gtest/gtest.h"
#include <array>
#include <cstdint>

namespace
{
    template<std::size_t Size>
    std::array<uint8_t, Size> Sequence()
    {
        std::array<uint8_t, Size> bytes{};
        for (std::size_t index = 0; index != bytes.size(); ++index)
            bytes[index] = static_cast<uint8_t>(index);
        return bytes;
    }
}

TEST(FlashWriteWindow, an_aligned_window_programs_all_words_little_endian)
{
    const auto bytes = Sequence<128>();
    infra::ConstByteRange buffer{ infra::MakeRange(bytes) };
    uint32_t address{ 0x80000 };

    const auto window = hal::tiva::TakeFlashWriteWindow(buffer, address);

    EXPECT_EQ(0x80000u, window.address);
    EXPECT_EQ(0xffffffffu, window.wordMask);
    EXPECT_EQ(0x03020100u, window.words[0]);
    EXPECT_EQ(0x7f7e7d7cu, window.words[31]);
    EXPECT_TRUE(buffer.empty());
    EXPECT_EQ(0x80080u, address);
}

TEST(FlashWriteWindow, a_window_stops_at_the_write_buffer_boundary)
{
    const auto bytes = Sequence<130>();
    infra::ConstByteRange buffer{ infra::MakeRange(bytes) };
    uint32_t address{ 0 };

    const auto first = hal::tiva::TakeFlashWriteWindow(buffer, address);
    const auto second = hal::tiva::TakeFlashWriteWindow(buffer, address);

    EXPECT_EQ(0u, first.address);
    EXPECT_EQ(0xffffffffu, first.wordMask);
    EXPECT_EQ(0x80u, second.address);
    EXPECT_EQ(0x1u, second.wordMask);
    EXPECT_EQ(0xffff8180u, second.words[0]);
    EXPECT_TRUE(buffer.empty());
    EXPECT_EQ(0x82u, address);
}

TEST(FlashWriteWindow, a_write_starting_in_the_last_word_spills_into_the_next_window)
{
    const auto bytes = Sequence<8>();
    infra::ConstByteRange buffer{ infra::MakeRange(bytes) };
    uint32_t address{ 0x7c };

    const auto first = hal::tiva::TakeFlashWriteWindow(buffer, address);
    const auto second = hal::tiva::TakeFlashWriteWindow(buffer, address);

    EXPECT_EQ(0u, first.address);
    EXPECT_EQ(0x80000000u, first.wordMask);
    EXPECT_EQ(0x03020100u, first.words[31]);
    EXPECT_EQ(0x80u, second.address);
    EXPECT_EQ(0x1u, second.wordMask);
    EXPECT_EQ(0x07060504u, second.words[0]);
}

TEST(FlashWriteWindow, bytes_outside_the_write_keep_the_erased_value)
{
    const std::array<uint8_t, 1> bytes{ 0xab };
    infra::ConstByteRange buffer{ infra::MakeRange(bytes) };
    uint32_t address{ 0x102 };

    const auto window = hal::tiva::TakeFlashWriteWindow(buffer, address);

    EXPECT_EQ(0x100u, window.address);
    EXPECT_EQ(0x1u, window.wordMask);
    EXPECT_EQ(0xffabffffu, window.words[0]);
    EXPECT_EQ(0xffffffffu, window.words[1]);
    EXPECT_EQ(0x103u, address);
}

TEST(FlashWriteWindow, an_unaligned_write_marks_only_the_words_it_touches)
{
    const auto bytes = Sequence<6>();
    infra::ConstByteRange buffer{ infra::MakeRange(bytes) };
    uint32_t address{ 0x3 };

    const auto window = hal::tiva::TakeFlashWriteWindow(buffer, address);

    EXPECT_EQ(0x7u, window.wordMask);
    EXPECT_EQ(0x00ffffffu, window.words[0]);
    EXPECT_EQ(0x04030201u, window.words[1]);
    EXPECT_EQ(0xffffff05u, window.words[2]);
}

TEST(FlashWriteWindow, an_empty_buffer_programs_nothing)
{
    infra::ConstByteRange buffer{};
    uint32_t address{ 0x40 };

    const auto window = hal::tiva::TakeFlashWriteWindow(buffer, address);

    EXPECT_EQ(0u, window.wordMask);
    EXPECT_EQ(0x40u, address);
}
