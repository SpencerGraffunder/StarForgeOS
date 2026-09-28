#ifndef HZ_SETTINGS_H
#define HZ_SETTINGS_H

#include <Arduino.h>

// Read Hertz Hunter's NVS settings store (namespace "settings") so the two
// firmwares on the same dual-boot board share one configuration. HHZ writes
// these from its menu (NuclearCounter src/settings.cpp); SFOS only reads.
//
//   b_index   : buzzer on/off  (0 = on (default), 1 = off)
//   b_a_index : battery alarm  (0 -> 3.6V, 1 -> 3.3V, 2 -> 3.0V)
namespace HzSettings {
    bool buzzerEnabled();     // true unless HHZ menu has the buzzer switched off
    int16_t batteryAlarmMv(); // alarm threshold in millivolts (default 3600)
}

#endif // HZ_SETTINGS_H
