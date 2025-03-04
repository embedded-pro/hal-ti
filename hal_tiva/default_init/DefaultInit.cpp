#include DEVICE_HEADER
#include "hal_tiva/cortex/InterruptCortex.hpp"
#include "hal_tiva/tiva/Gpio.hpp"
#if defined(TM4C123)
#include "hal_tiva/tiva/PinoutTableDefaultTm4c123.hpp"
#elif defined(TM4C129)
#include "hal_tiva/tiva/PinoutTableDefaultTm4c129.hpp"
#else
#error "MCU family not defined or invalid [TM4C123 | TM4C129]!"
#endif
#include <errno.h>
#include <sys/types.h>

extern "C"
{
    extern char _Heap_Begin;
    extern char _Heap_Limit;

    caddr_t _sbrk(int incr)
    {
        static char* current_heap_end = &_Heap_Begin;

        char* current_block_address = current_heap_end;

        incr = (incr + 3) & (~3);
        if (current_heap_end + incr > &_Heap_Limit)
        {
            errno = ENOMEM;
            return reinterpret_cast<caddr_t>(-1);
        }

        current_heap_end += incr;

        return static_cast<caddr_t>(current_block_address);
    }

    [[gnu::weak]] void Default_Handler_Forwarded()
    {
        hal::InterruptTable::Instance().Invoke(hal::ActiveInterrupt());
    }

    void abort()
    {
        __BKPT();
        NVIC_SystemReset();
        __builtin_unreachable();
    }

    void _init()
    {}

    void __assert_func(const char*, int, const char*, const char*)
    {
        std::abort();
    }

    void assert_failed(uint8_t* file, uint32_t line)
    {
        std::abort();
    }

    void HardwareInitialization()
    {
        static hal::InterruptTable::WithStorage<128> interruptTable;
        static hal::tiva::Gpio gpio{ hal::tiva::pinoutTableDefault, hal::tiva::analogTableDefault };
    }
}
