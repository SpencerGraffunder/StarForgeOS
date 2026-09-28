#include "nc_settings.h"
#include <Preferences.h>

namespace NcSettings {

// Same namespace + keys as NuclearCounter Settings (src/settings.cpp):
//   preferences.begin("settings", true)
//   getInt("b_index", 0)      buzzer: 0 = on, 1 = off
//   getInt("b_a_index", 0)    alarm: -3*idx + 36, in 0.1V units
static const char* const NS = "settings";

bool buzzerEnabled() {
    Preferences p;
    if (!p.begin(NS, true)) {
        return true;  // can't read store -> NuclearCounter default (on)
    }
    int idx = p.getInt("b_index", 0);
    p.end();
    return idx == 0;
}

int16_t batteryAlarmMv() {
    Preferences p;
    if (!p.begin(NS, true)) {
        return 3600;  // NuclearCounter default (3.6V)
    }
    int idx = p.getInt("b_a_index", 0);
    p.end();
    return static_cast<int16_t>((36 - 3 * idx) * 100);  // 0.1V units -> mV
}

}  // namespace NcSettings
