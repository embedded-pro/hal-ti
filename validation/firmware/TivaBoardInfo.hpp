#ifndef VALIDATION_TIVA_BOARD_INFO_HPP
#define VALIDATION_TIVA_BOARD_INFO_HPP

#include "services/hil/BoardInfo.hpp"

namespace validation
{
    const char* ReadAndClearResetCause();

    class TivaBoardInfo
        : public services::hil::BoardInfo
    {
    public:
        explicit TivaBoardInfo(const char* resetCause);

        const char* Name() const override;
        const char* Family() const override;
        uint32_t SystemClock() const override;
        const char* ResetCause() const override;
        infra::ConstByteRange UniqueId() const override;

    private:
        const char* resetCause;
    };
}

#endif
