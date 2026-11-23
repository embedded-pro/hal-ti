#pragma once

#include "hal/interfaces/MacAddress.hpp"
#include "hal/synchronous_interfaces/SynchronousRandomDataGenerator.hpp"
#include <array>
#include <cstdint>

namespace demo
{
    hal::MacAddress BoardMacAddress();

    class UniqueIdRandomGenerator
        : public hal::SynchronousRandomDataGenerator
    {
    public:
        UniqueIdRandomGenerator();

        void GenerateRandomData(infra::ByteRange result) override;

    private:
        uint32_t Next();

    private:
        std::array<uint32_t, 4> state{ { 0x9e3779b9, 0x243f6a88, 0xb7e15162, 0xdeadbeef } };
    };
}
