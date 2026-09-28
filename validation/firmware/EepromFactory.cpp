#include "validation/firmware/EepromFactory.hpp"

namespace validation
{
    hal::Eeprom& TivaEepromFactory::Instance()
    {
        if (!eeprom)
            eeprom.emplace();

        return *eeprom;
    }
}
