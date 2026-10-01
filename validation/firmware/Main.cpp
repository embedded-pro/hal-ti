#include "BoardProfile.hpp"
#include "hal/cortex_m/Reset.hpp"
#include "hal_tiva/instantiations/EventInfrastructure.hpp"
#include "services/hil/HilCommand.hpp"
#include "services/hil/HilPinNaming.hpp"
#include "services/hil/HilPinPool.hpp"
#include "services/hil/HilSystemCommands.hpp"
#include "services/hil/commands/HilAdcCommands.hpp"
#include "services/hil/commands/HilCanCommands.hpp"
#include "services/hil/commands/HilComparatorCommands.hpp"
#include "services/hil/commands/HilEepromCommands.hpp"
#include "services/hil/commands/HilGpioCommands.hpp"
#include "services/hil/commands/HilPwmCommands.hpp"
#include "services/hil/commands/HilQeiCommands.hpp"
#include "services/hil/commands/HilSpiCommands.hpp"
#include "services/hil/commands/HilUartCommands.hpp"
#include "services/hil/commands/HilWatchDogCommands.hpp"
#include "services/peripheral/DebugLed.hpp"
#include "validation/firmware/AdcFactory.hpp"
#include "validation/firmware/CanFactory.hpp"
#include "validation/firmware/ComparatorFactory.hpp"
#include "validation/firmware/Console.hpp"
#include "validation/firmware/EepromFactory.hpp"
#include "validation/firmware/EthernetGroup.hpp"
#include "validation/firmware/PwmFactory.hpp"
#include "validation/firmware/QeiFactory.hpp"
#include "validation/firmware/SpiFactory.hpp"
#include "validation/firmware/TivaBoardInfo.hpp"
#include "validation/firmware/TivaPinFactory.hpp"
#include "validation/firmware/UartFactory.hpp"
#include "validation/firmware/WatchDogFactory.hpp"

int main()
{
    static validation::TivaBoardInfo boardInfo{ validation::ReadAndClearResetCause() };
    validation::board::InitializeClocks();

    static instantiations::EventInfrastructure eventInfrastructure;
    static validation::Console console;
    static hal::tiva::GpioPin debugLedPin{ validation::PortOf(validation::board::debugLed), validation::board::debugLed.index };
    static services::DebugLed debugLed{ debugLedPin, std::chrono::milliseconds(100), std::chrono::milliseconds(1400) };
    static validation::TivaPinFactory pinFactory;
    static services::HilPinPool::WithCapacity<validation::TivaPinFactory::capacity> pins{ pinFactory, infra::MakeRange(validation::board::reservedPins) };
    static services::HilPinNamingDefault naming{ validation::board::portLetters, validation::board::maximumPinIndex, infra::MakeRange(validation::board::aliases) };
    static services::HilContext context{ console.response, pins, naming, console.terminal };

    static hal::cortex::Reset reset;
    static services::HilSystemCommands system{ context, boardInfo, reset };
    static services::HilGpioCommands::WithMaxPins<8> gpio{ context };

    static validation::TivaPwmFactory pwmFactory{ naming, console.response, pins };
    static services::HilPwmCommands pwm{ context, pwmFactory };
    static validation::PwmExtensionCommands pwmExtension{ context, pwmFactory };

    static validation::TivaUartFactory uartFactory{ naming, console.dma };
    static services::HilUartCommands::WithCapacity<256, 112> uart{ context, uartFactory };

    static validation::TivaSpiFactory spiFactory{ naming };
    static services::HilSpiCommands::WithCapacity<64> spi{ context, spiFactory };

    static validation::TivaAdcFactory adcFactory{ naming };
    static services::HilAdcCommands::WithCapacity<validation::TivaAdcFactory::sequencers, 64> adc{ context, adcFactory };

    static validation::TivaComparatorFactory comparatorFactory{ naming };
    static services::HilComparatorCommands comparator{ context, comparatorFactory };

    static validation::TivaQeiFactory qeiFactory{ naming };
    static services::HilQeiCommands qei{ context, qeiFactory };

    static validation::TivaCanFactory canFactory{ naming };
    static services::HilCanCommands can{ context, canFactory };

    static validation::TivaEepromFactory eepromFactory;
    static services::HilEepromCommands::WithCapacity<112> eeprom{ context, eepromFactory };

    static validation::TivaWatchDogFactory watchDogFactory{ naming, pins };
    static services::HilWatchDogCommands watchDog{ context, watchDogFactory };

    validation::CreateEthernetGroup(context);

    system.PrintBoot();
    eventInfrastructure.Run();
    __builtin_unreachable();
}
