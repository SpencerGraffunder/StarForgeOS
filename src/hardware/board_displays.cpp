#include "board_displays.h"
#include <WiFi.h>

BoardDisplays::BoardDisplays() {
    // Constructor
}

#if defined(BOARD_NUCLEARCOUNTER)
#include <Wire.h>
#include <U8g2lib.h>
#include "sh1306.h"      // SH1306 driver (same panels as Hertz Hunter firmware)
#include <esp_ota_ops.h>

// ---------------------------------------------------------------------------
// NuclearCounter 1.3" OLED (SH1306) + 3-button menu (standalone mode only)
//
// Buttons (same wiring as the Hertz Hunter firmware: INPUT_PULLDOWN, the pin
// reads HIGH while pressed):
//   prev  = NC_PREV_BUTTON_PIN (up)
//   next  = NC_NEXT_BUTTON_PIN (down)
//   select= MODE_SWITCH_PIN (activate / confirm)
//
// The panels are SH1306 with the known per-panel right-edge glass defect, so
// this mirrors the Hertz Hunter workaround: image at x_offset 0 (native
// SH1306 layout), full-RAM clear at boot, and nothing drawn into image
// columns 122-127 (6px right guard).
// ---------------------------------------------------------------------------

// Dual-boot: switch to the Hertz Hunter app (ota_0 slot, see
// partitions_dualboot.csv) and reboot. The shared bootloader + otadata
// partition then boot the last-selected app.
static void bootHertzHunter() {
    const esp_partition_t *hertz = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
    if (hertz != nullptr && esp_ota_set_boot_partition(hertz) == ESP_OK) {
        // Small delay so the last screen update can flush over I2C
        delay(100);
        esp_restart();
    }
    // No valid ota_0 slot: fall through, stay in StarForge.
}

enum NcScreen {
    NC_SCREEN_STATUS = 0,
    NC_SCREEN_BOOT_HHZ = 1,
    NC_SCREEN_CONFIRM
};

static U8G2_SH1306_128X64_NONAME_F_HW_I2C ncDisplay(
    U8G2_R0,
    /* reset = */ U8X8_PIN_NONE,
    /* clock = */ LCD_I2C_SCL,
    /* data  = */ LCD_I2C_SDA
);

static NcScreen nc_screen = NC_SCREEN_STATUS;
static unsigned long nc_last_redraw = 0;

// Edge-detecting button reader (active high, pulled down). Returns true
// exactly once per press; ignores edges within 150ms of the last accepted
// edge (debounce).
static bool ncButtonEdge(uint8_t pin, bool &wasHigh, unsigned long &lastEdgeMs) {
    bool high = (digitalRead(pin) == HIGH);
    bool edge = high && !wasHigh;
    wasHigh = high;
    if (!edge) {
        return false;
    }
    unsigned long now = millis();
    if (now - lastEdgeMs < 150) {
        return false;  // debounce
    }
    lastEdgeMs = now;
    return true;
}

void BoardDisplays::initNuclearCounter(const String& ssid) {
    ncDisplay.begin();

    // SH1306 panel workaround (mirrors Hertz Hunter Menu::begin): park the
    // image at x_offset 0 so the two bad rightmost glass columns are
    // unaddressed, and clear the full RAM twice so they show black.
    for (int pass = 0; pass < 2; pass++) {
        ncDisplay.getU8x8()->x_offset = 0;
        ncDisplay.clearDisplay();
        ncDisplay.getU8x8()->x_offset = 2;
        ncDisplay.clearDisplay();
        ncDisplay.getU8x8()->x_offset = 4;
        ncDisplay.clearDisplay();
        ncDisplay.getU8x8()->x_offset = 0;
        delay(50);
    }

    pinMode(NC_PREV_BUTTON_PIN, INPUT_PULLDOWN);
    pinMode(NC_NEXT_BUTTON_PIN, INPUT_PULLDOWN);
    pinMode(MODE_SWITCH_PIN, INPUT_PULLDOWN);

    ncDisplay.clearBuffer();
    ncDisplay.setFont(u8g2_font_5x8_tf);
    ncDisplay.drawStr(4, 30, "Starting...");
    ncDisplay.sendBuffer();
    nc_last_redraw = millis();
}

void BoardDisplays::processNuclearCounter(const String& ssid) {
    (void)ssid;

    static bool prevWasHigh = false; static unsigned long prevEdgeMs = 0;
    static bool nextWasHigh = false; static unsigned long nextEdgeMs = 0;
    static bool selWasHigh = false;  static unsigned long selEdgeMs = 0;

    bool prevEdge = ncButtonEdge(NC_PREV_BUTTON_PIN, prevWasHigh, prevEdgeMs);
    bool nextEdge = ncButtonEdge(NC_NEXT_BUTTON_PIN, nextWasHigh, nextEdgeMs);
    bool selectEdge = ncButtonEdge(MODE_SWITCH_PIN, selWasHigh, selEdgeMs);

    if (nc_screen == NC_SCREEN_CONFIRM) {
        if (selectEdge) {
            bootHertzHunter();  // does not return
        }
        if (prevEdge || nextEdge) {
            nc_screen = NC_SCREEN_STATUS;  // cancel
        }
    } else {
        // Menu navigation
        if (prevEdge) {
            nc_screen = (nc_screen == NC_SCREEN_STATUS) ? NC_SCREEN_BOOT_HHZ
                                                        : NC_SCREEN_STATUS;
        }
        if (nextEdge) {
            nc_screen = (nc_screen == NC_SCREEN_BOOT_HHZ) ? NC_SCREEN_STATUS
                                                          : NC_SCREEN_BOOT_HHZ;
        }
        if (selectEdge) {
            if (nc_screen == NC_SCREEN_BOOT_HHZ) {
                nc_screen = NC_SCREEN_CONFIRM;
            }
            // nc_screen == NC_SCREEN_STATUS: nothing to do
        }
    }

    // Redraw on state change, or every second (IP / status refresh)
    unsigned long now = millis();
    if (now - nc_last_redraw < 1000) {
        return;
    }
    nc_last_redraw = now;

    ncDisplay.clearBuffer();
    ncDisplay.setFont(u8g2_font_7x13B_tf);
    ncDisplay.drawStr(2, 12, "StarForgeOS");

    if (nc_screen == NC_SCREEN_CONFIRM) {
        ncDisplay.setFont(u8g2_font_7x13_tf);
        ncDisplay.drawStr(2, 32, "Boot HertzHunter?");
        ncDisplay.setFont(u8g2_font_5x7_tf);
        ncDisplay.drawStr(2, 46, "select = yes");
        ncDisplay.drawStr(2, 57, "prev   = no");
    } else {
        ncDisplay.setFont(u8g2_font_5x7_tf);
        char ipLine[20];
        snprintf(ipLine, sizeof(ipLine), "IP: %s", WiFi.softAPIP().toString().c_str());
        ncDisplay.drawStr(2, 20, ipLine);

        // Menu rows: 16px tall, selected row inverted (6px right guard:
        // box width 122 = image columns 0..121 only)
        int row = (nc_screen == NC_SCREEN_STATUS) ? 0 : 1;
        for (int i = 0; i < 2; i++) {
            if (i == row) {
                ncDisplay.setDrawColor(1);
                ncDisplay.drawBox(0, 26 + i * 16, 122, 16);
                ncDisplay.setDrawColor(0);
                ncDisplay.drawStr(6, 38 + i * 16, (i == 0) ? "Status" : "Boot HertzHunter");
                ncDisplay.setDrawColor(1);
            } else {
                ncDisplay.drawStr(6, 38 + i * 16, (i == 0) ? "Status" : "Boot HertzHunter");
            }
        }
    }

    ncDisplay.sendBuffer();
}

#endif  // BOARD_NUCLEARCOUNTER
