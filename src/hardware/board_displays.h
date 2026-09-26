#ifndef BOARD_DISPLAYS_H
#define BOARD_DISPLAYS_H

#include <Arduino.h>
#include "config/config.h"

// Board-specific display initialization and UI
// NuclearCounter: 1.3" I2C OLED (SH1306) with 3-button menu in standalone mode
class BoardDisplays {
public:
    BoardDisplays();

#if defined(BOARD_NUCLEARCOUNTER)
    // Initialize NuclearCounter 1.3" OLED (SH1306 128x64) and the 3-button menu.
    void initNuclearCounter(const String& ssid);

    // Poll buttons + refresh the menu. Call from StandaloneMode::process().
    void processNuclearCounter(const String& ssid);
#endif

private:
    // No state needed - static display initialization only
};

#endif // BOARD_DISPLAYS_H
