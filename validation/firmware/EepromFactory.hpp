#ifndef VALIDATION_EEPROM_FACTORY_HPP
#define VALIDATION_EEPROM_FACTORY_HPP

#include "hal_tiva/tiva/Eeprom.hpp"
#include "services/hil/commands/HilEepromCommands.hpp"
#include <optional>

namespace validation
{
    class TivaEepromFactory
        : public services::HilEepromFactory
    {
    public:
        hal::Eeprom& Instance() override;

    private:
        std::optional<hal::tiva::Eeprom> eeprom;
    };
}

#endif
