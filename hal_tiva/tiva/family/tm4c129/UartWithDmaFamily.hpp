#pragma once

#include "hal_tiva/tiva/Dma.hpp"
#include <cstdint>

namespace hal::tiva::family
{
    constexpr uint32_t DmaTxClearMask = 0x00020000; // UART_ICR_DMATXIC
    constexpr uint32_t DmaRxClearMask = 0x00010000; // UART_ICR_DMARXIC
    constexpr uint32_t DmaTxStatusBit = 0x00020000; // UART_RIS_DMATXRIS
    constexpr uint32_t DmaRxStatusBit = 0x00010000; // UART_RIS_DMARXRIS

    inline bool DmaTxComplete(DmaChannel&, uint32_t rawStatus)
    {
        return (rawStatus & DmaTxStatusBit) != 0;
    }

    inline void ClearDmaTx(DmaChannel&)
    {
    }

    inline bool DmaRxComplete(DmaChannel&, uint32_t rawStatus)
    {
        return (rawStatus & DmaRxStatusBit) != 0;
    }

    inline void ClearDmaRx(DmaChannel&)
    {
    }
}
