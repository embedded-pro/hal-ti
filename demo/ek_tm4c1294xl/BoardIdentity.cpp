#include "BoardIdentity.hpp"
#include "hal_tiva/tiva/UniqueDeviceId.hpp"
#include DEVICE_HEADER

namespace demo
{
    namespace
    {
        constexpr uint32_t blankUserRegister = 0xffffffff;

        hal::MacAddress LocallyAdministeredAddress()
        {
            hal::MacAddress address{ { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 } };
            auto uid = hal::tiva::UniqueDeviceId();

            for (std::size_t i = 0; i != uid.size(); ++i)
                address[2 + i % 4] ^= uid[i];

            return address;
        }
    }

    hal::MacAddress BoardMacAddress()
    {
        const uint32_t user0 = FLASH_CTRL->USERREG0;
        const uint32_t user1 = FLASH_CTRL->USERREG1;

        if (user0 == blankUserRegister && user1 == blankUserRegister)
            return LocallyAdministeredAddress();

        return { { static_cast<uint8_t>(user0), static_cast<uint8_t>(user0 >> 8), static_cast<uint8_t>(user0 >> 16),
            static_cast<uint8_t>(user1), static_cast<uint8_t>(user1 >> 8), static_cast<uint8_t>(user1 >> 16) } };
    }

    UniqueIdRandomGenerator::UniqueIdRandomGenerator()
    {
        auto uid = hal::tiva::UniqueDeviceId();

        for (std::size_t i = 0; i != uid.size(); ++i)
        {
            uint32_t& word = state[i % state.size()];
            word = ((word << 8) | (word >> 24)) ^ uid[i];
        }

        for (std::size_t i = 0; i != 16; ++i)
            Next();
    }

    void UniqueIdRandomGenerator::GenerateRandomData(infra::ByteRange result)
    {
        uint32_t word = 0;
        std::size_t available = 0;

        for (uint8_t& byte : result)
        {
            if (available == 0)
            {
                word = Next();
                available = sizeof(word);
            }

            byte = static_cast<uint8_t>(word);
            word >>= 8;
            --available;
        }
    }

    uint32_t UniqueIdRandomGenerator::Next()
    {
        uint32_t t = state[3];
        const uint32_t s = state[0];

        state[3] = state[2];
        state[2] = state[1];
        state[1] = s;

        t ^= t << 11;
        t ^= t >> 8;
        state[0] = t ^ s ^ (s >> 19);

        return state[0];
    }
}
