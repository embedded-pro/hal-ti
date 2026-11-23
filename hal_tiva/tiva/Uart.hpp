#pragma once

#include "hal_tiva/tiva/UartBase.hpp"

namespace hal::tiva
{
    class Uart
        : public UartBase
    {
    public:
        using UartBase::UartBase;

        void SendData(infra::MemoryRange<const uint8_t> data, infra::Function<void()> actionOnCompletion = infra::emptyFunction) override;
        void ReceiveData(infra::Function<void(infra::ConstByteRange data)> dataReceived) override;

    private:
        void Invoke() override;
    };
}
