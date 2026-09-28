#include "BoardProfile.hpp"
#include "hal/cortex_m/Reset.hpp"
#include "hal_tiva/instantiations/EventInfrastructure.hpp"
#include "services/hil/Command.hpp"
#include "services/hil/PinNaming.hpp"
#include "services/hil/PinPool.hpp"
#include "services/hil/SystemCommands.hpp"
#include "services/hil/commands/AdcCommands.hpp"
#include "services/hil/commands/CanCommands.hpp"
#include "services/hil/commands/ComparatorCommands.hpp"
#include "services/hil/commands/EepromCommands.hpp"
#include "services/hil/commands/GpioCommands.hpp"
#include "services/hil/commands/PwmCommands.hpp"
#include "services/hil/commands/QeiCommands.hpp"
#include "services/hil/commands/SpiCommands.hpp"
#include "services/hil/commands/UartCommands.hpp"
#include "services/hil/commands/WatchDogCommands.hpp"
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
    static validation::TivaPinFactory pinFactory;
    static services::hil::PinPool::WithCapacity<validation::TivaPinFactory::capacity> pins{ pinFactory, infra::MakeRange(validation::board::reservedPins) };
    static services::hil::PinNamingDefault naming{ validation::board::portLetters, validation::board::maximumPinIndex, infra::MakeRange(validation::board::aliases) };
    static services::hil::Context context{ console.response, pins, naming, console.terminal };

    static hal::cortex::Reset reset;
    static services::hil::SystemCommands system{ context, boardInfo, reset };
    static services::hil::GpioCommands::WithMaxPins<8> gpio{ context };

    static validation::TivaPwmFactory pwmFactory{ naming, console.response };
    static services::hil::PwmCommands pwm{ context, pwmFactory };
    static validation::PwmExtensionCommands pwmExtension{ context, pwmFactory };

    static validation::TivaUartFactory uartFactory{ naming, console.dma };
    static services::hil::UartCommands::WithCapacity<256, 112> uart{ context, uartFactory };

    static validation::TivaSpiFactory spiFactory{ naming };
    static services::hil::SpiCommands::WithCapacity<64> spi{ context, spiFactory };

    static validation::TivaAdcFactory adcFactory{ naming };
    static services::hil::AdcCommands::WithCapacity<validation::TivaAdcFactory::sequencers, 64> adc{ context, adcFactory };

    static validation::TivaComparatorFactory comparatorFactory{ naming };
    static services::hil::ComparatorCommands comparator{ context, comparatorFactory };

    static validation::TivaQeiFactory qeiFactory{ naming };
    static services::hil::QeiCommands qei{ context, qeiFactory };

    static validation::TivaCanFactory canFactory{ naming };
    static services::hil::CanCommands can{ context, canFactory };

    static validation::TivaEepromFactory eepromFactory;
    static services::hil::EepromCommands::WithCapacity<112> eeprom{ context, eepromFactory };

    static validation::TivaWatchDogFactory watchDogFactory;
    static services::hil::WatchDogCommands watchDog{ context, watchDogFactory };

    validation::CreateEthernetGroup(context);

    system.PrintBoot();
    eventInfrastructure.Run();
    __builtin_unreachable();
}
