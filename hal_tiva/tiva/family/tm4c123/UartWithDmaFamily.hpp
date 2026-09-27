#ifndef HAL_UART_WITH_DMA_FAMILY_TM4C123_HPP
#define HAL_UART_WITH_DMA_FAMILY_TM4C123_HPP

#include DEVICE_HEADER
#include "hal_tiva/tiva/Dma.hpp"
#include <cstdint>

namespace hal::tiva::family
{
    // The TM4C123 UART has no DMA status bits; uDMA completion is raised on the UART vector and acknowledged in DMACHIS
    constexpr uint32_t DmaTxClearMask = 0;
    constexpr uint32_t DmaRxClearMask = 0;

    inline bool DmaTxComplete(DmaChannel& dma, uint32_t)
    {
        return (UDMA->CHIS & (1u << dma.ChannelNumber())) != 0;
    }

    inline void ClearDmaTx(DmaChannel& dma)
    {
        UDMA->CHIS = 1u << dma.ChannelNumber();
    }

    inline bool DmaRxComplete(DmaChannel& dma, uint32_t)
    {
        return (UDMA->CHIS & (1u << dma.ChannelNumber())) != 0;
    }

    inline void ClearDmaRx(DmaChannel& dma)
    {
        UDMA->CHIS = 1u << dma.ChannelNumber();
    }
}

#endif
