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
    // Set by main() at boot (raw mode-pin level before mode decision) so the
    // splash screens can show it for dual-boot debugging.
    static uint8_t bootSelectHigh;
    // Initialize NuclearCounter 1.3" OLED (SH1306 128x64) and the 3-button menu.
    void initNuclearCounter(const String& ssid);

    // Poll buttons + refresh the menu. Call from StandaloneMode::process().
    void processNuclearCounter(const String& ssid);

    // Initialize the OLED for RotorHazard USB node mode (shows the node menu,
    // home row "USB Node" selected).
    void initNuclearCounterNodeMode();

    // Node-mode menu: poll buttons + refresh. Rows: USB Node (home),
    // Standalone, Boot HertzHunter. Same selectable style as the standalone
    // menu. Called from NodeMode::process().
    void processNuclearCounterNode();

    // Switch the boot slot to Hertz Hunter (ota_0) and reboot. Does not return.
    void bootHertzHunter();

    // Enter USB node (RotorHazard) mode: set the node-mode NVS flag and reboot.
    // Does not return. (Called from standalone mode's "USB Node Mode" row.)
    void bootNodeMode();

    // Return to standalone (timer) mode: clear the node-mode NVS flag and
    // reboot. Does not return. (Called from node mode's "Standalone" row.)
    void bootStandalone();
#endif

private:
    // Shared selectable-menu handler for both standalone and node mode.
    // Row 0 = info (standalone: WiFi IP; node: "USB Node Mode" indicator),
    // rows 1+2 = selectable (mode switch + Boot HertzHunter). isNode picks the
    // labels and the mode-switch action.
    void ncMenuProcess(bool isNode);
};

#endif // BOARD_DISPLAYS_H
