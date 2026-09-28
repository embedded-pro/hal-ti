#include "BoardProfile.hpp"
#include "hal_tiva/instantiations/EventInfrastructure.hpp"
#include "validation/firmware/AdcCommands.hpp"
#include "validation/firmware/CanCommands.hpp"
#include "validation/firmware/ComparatorCommands.hpp"
#include "validation/firmware/Console.hpp"
#include "validation/firmware/Context.hpp"
#include "validation/firmware/EepromCommands.hpp"
#include "validation/firmware/EthernetCommands.hpp"
#include "validation/firmware/GpioCommands.hpp"
#include "validation/firmware/PinPool.hpp"
#include "validation/firmware/PwmCommands.hpp"
#include "validation/firmware/QeiCommands.hpp"
#include "validation/firmware/SpiCommands.hpp"
#include "validation/firmware/SystemCommands.hpp"
#include "validation/firmware/UartCommands.hpp"
#include "validation/firmware/WatchDogCommands.hpp"

int main()
{
    const char* resetCause = validation::ReadAndClearResetCause();
    validation::board::InitializeClocks();

    static instantiations::EventInfrastructure eventInfrastructure;
    static validation::Console console;
    static validation::PinPool pins;
    static validation::Context context{ console.response, pins, console.dma, console.terminal };

    static validation::SystemCommands system{ context, resetCause };
    static validation::GpioCommands gpio{ context };
    static validation::PwmCommands pwm{ context };
    static validation::UartCommands uart{ context };
    static validation::SpiCommands spi{ context };
    static validation::AdcCommands adc{ context };
    static validation::ComparatorCommands comparator{ context };
    static validation::QeiCommands qei{ context };
    static validation::CanCommands can{ context };
    static validation::EepromCommands eeprom{ context };
    static validation::WatchDogCommands watchDog{ context };
    static validation::EthernetCommands ethernet{ context };

    system.PrintBoot();
    eventInfrastructure.Run();
    __builtin_unreachable();
}
