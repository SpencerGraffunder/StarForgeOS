#ifndef NC_SETTINGS_H
#define NC_SETTINGS_H

#include <Arduino.h>

// Read the NuclearCounter firmware's NVS settings store (namespace
// "settings") so the two firmwares on the same dual-boot board share one
// configuration. NuclearCounter writes these from its menu (NuclearCounter
// repo src/settings.cpp); StarForgeOS only reads them.
//
//   b_index   : buzzer on/off  (0 = on (default), 1 = off)
//   b_a_index : battery alarm  (0 -> 3.6V, 1 -> 3.3V, 2 -> 3.0V)
namespace NcSettings {
    bool buzzerEnabled();      // true unless the NuclearCounter menu has the buzzer switched off
    int16_t batteryAlarmMv(); // alarm threshold in millivolts (default 3600)
}

#endif // NC_SETTINGS_H
