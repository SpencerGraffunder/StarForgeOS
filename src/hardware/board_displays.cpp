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
#include "config/config_loader.h"
#include "hardware/nc_buzzer.h"
#include "hardware/nc_battery.h"
#include "settings/hz_settings.h"

// Raw mode-pin (SELECT) level captured at boot, for splash diagnostics.
uint8_t BoardDisplays::bootSelectHigh = 0;

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

static U8G2_SH1306_128X64_NONAME_F_HW_I2C ncDisplay(
    U8G2_R0,
    /* reset = */ U8X8_PIN_NONE,
    /* clock = */ LCD_I2C_SCL,
    /* data  = */ LCD_I2C_SDA
);

// Dual-boot: switch to the Hertz Hunter app (ota_0 slot, see
// partitions_dualboot.csv) and reboot. The shared bootloader + otadata
// partition then boot the last-selected app.
void BoardDisplays::bootHertzHunter() {
    // Tell the user what's happening before the reboot.
    ncDisplay.clearBuffer();
    ncDisplay.setFont(u8g2_font_7x13B_tf);
    ncDisplay.drawStr(2, 20, "StarForgeOS");
    ncDisplay.setFont(u8g2_font_7x13_tf);
    ncDisplay.drawStr(2, 40, "Booting Hertz Hunter");
    ncDisplay.drawStr(2, 54, "...");
    ncDisplay.sendBuffer();

    const esp_partition_t *hertz = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
    Serial.printf("[SFOS] bootHertzHunter: ota_0 = %s @0x%lx\n",
                  hertz ? hertz->label : "NULL", hertz ? (unsigned long)hertz->address : 0UL);
    if (hertz != nullptr) {
        esp_err_t err = esp_ota_set_boot_partition(hertz);
        Serial.printf("[SFOS] esp_ota_set_boot_partition -> %s (0x%x)\n", esp_err_to_name(err), (unsigned)err);
        if (err == ESP_OK) {
            // Small delay so the last screen update can flush over I2C
            delay(200);
            esp_restart();
        }
    }
    // No valid ota_0 slot: fall through, stay in StarForge.
}

// Buzzer + battery: same pins as the Hertz Hunter firmware. Buzzer on/off is
// read from HHZ's NVS settings (configured in HHZ's menu), so both firmwares
// on the board share one configuration.
static NcBuzzer ncBuzzer(BUZZER_PIN);
static NcBattery ncBattery(BATTERY_PIN, BATTERY_VOLTAGE_OFFSET);

// ---------------------------------------------------------------------------
// Unified selectable menu, shared by standalone mode and node mode.
//
//   Row 0 (info, NOT selectable):
//     - standalone : WiFi AP address (IP [+ SSID])
//     - node       : "USB Node Mode" (tells you which mode you're in)
//   Row 1 (selectable) : mode switch
//     - standalone : "USB Node Mode"  -> enter node (RotorHazard) mode
//     - node       : "Standalone"     -> back to standalone (timer) mode
//   Row 2 (selectable) : "Boot HertzHunter" -> dual-boot to Hertz Hunter
//
// The selector sits on rows 1/2 only (row 0 is display-only). It starts on
// row 1; prev/next toggles the two selectable rows; select opens a confirm
// overlay (select = yes, prev/next = no) that then performs the action.
// ---------------------------------------------------------------------------
static bool  nc_sel = 0;               // 0 = mode-switch row, 1 = HertzHunter row
static bool  nc_confirm = false;       // confirm overlay showing
static bool  nc_confirm_hhz = false;   // pending action is "Boot HertzHunter"
static bool  nc_prevHi = false, nc_nextHi = false, nc_selHi = false;
static unsigned long nc_prevMs = 0, nc_nextMs = 0, nc_selMs = 0;
static int   nc_lastSel = -1;          // for redraw throttling
static bool  nc_lastCfm = false;
static unsigned long nc_last_redraw = 0;

// Info line for standalone mode: the WiFi AP address the user connects to.
static String ncStandaloneInfoLine() {
    String ip = WiFi.softAPIP().toString();
    if (ip == "0.0.0.0") {
        return String("WiFi starting...");
    }
    String ssid = WiFi.SSID();
    String line = (ssid.length() > 0) ? ("IP " + ip + "  " + ssid) : ("IP " + ip);
    if (line.length() > 24) {
        line = "IP " + ip;
    }
    return line;
}

// Draw the unified menu (or the confirm overlay).
static void ncDrawMenu(const String& infoLine, const char* switchLabel,
                       bool confirm, const char* confirmMsg) {
    ncDisplay.clearBuffer();
    ncDisplay.setDrawColor(1);  // white text on black background
    ncDisplay.setFont(u8g2_font_7x13B_tf);
    ncDisplay.drawStr(2, 12, "StarForgeOS");

    if (confirm) {
        ncDisplay.setFont(u8g2_font_7x13_tf);
        ncDisplay.drawStr(2, 34, confirmMsg);
        ncDisplay.setFont(u8g2_font_5x7_tf);
        ncDisplay.drawStr(2, 50, "select = yes");
        ncDisplay.drawStr(2, 62, "prev   = no");
    } else {
        // Row 0: info line (display only, not selectable)
        ncDisplay.setFont(u8g2_font_5x7_tf);
        ncDisplay.drawStr(2, 24, infoLine.c_str());
        // Rows 1 + 2: selectable (14px boxes at y=28 and y=42, text at +10)
        const char* labels[2] = { switchLabel, "Boot HertzHunter" };
        for (int i = 0; i < 2; i++) {
            int boxTop = 28 + i * 14;
            if (i == nc_sel) {
                ncDisplay.setDrawColor(1);
                ncDisplay.drawBox(0, boxTop, 122, 14);   // white highlight bar
                ncDisplay.setDrawColor(0);               // black text on white
            } else {
                ncDisplay.setDrawColor(1);               // white text on black
            }
            ncDisplay.setFont(u8g2_font_5x7_tf);
            ncDisplay.drawStr(6, boxTop + 10, labels[i]);
        }
    }

    // Battery voltage, bottom-right (HHZ style: "3.7v"). Right-aligned to
    // end at x=118 so the text never reaches the bad-glass columns (122+).
    // NOTE: set white explicitly - the row loop above may have left the
    // draw color black (selected-row text), which made the voltage
    // black-on-black whenever the second row was the selected one.
    ncDisplay.setDrawColor(1);
    int mv = ncBattery.voltageMv();
    if (mv < 0) mv = 0;
    if (mv > 19900) mv = 19900;
    int tenths = (mv + 50) / 100;   // round to 0.1V (same value HHZ shows)
    char vbuf[8];
    snprintf(vbuf, sizeof(vbuf), "%d.%dv", tenths / 10, tenths % 10);
    ncDisplay.setFont(u8g2_font_5x7_tf);
    int vw = ncDisplay.getStrWidth(vbuf);
    ncDisplay.drawStr(118 - vw, 64, vbuf);

    ncDisplay.sendBuffer();
}

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

// Common OLED bring-up, shared by standalone menu and node-mode splash.
// (SH1306 panel workaround: mirror the Hertz Hunter Menu::begin — park the
// image at x_offset 0 so the bad rightmost glass columns are unaddressed,
// and clear the full RAM twice so they show black.)
static void ncCommonInit() {
    ncDisplay.begin();
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
}

void BoardDisplays::initNuclearCounter(const String& ssid) {
    (void)ssid;
    ncCommonInit();

    ncDisplay.clearBuffer();
    ncDisplay.setFont(u8g2_font_7x13B_tf);
    ncDisplay.drawStr(4, 24, "StarForgeOS");
    ncDisplay.setFont(u8g2_font_5x7_tf);
    ncDisplay.drawStr(4, 44, "Starting...");
    ncDisplay.sendBuffer();
    nc_last_redraw = millis();

    // Initialisation-complete buzz, gated by HHZ's buzzer setting.
    if (HzSettings::buzzerEnabled()) {
        ncBuzzer.buzz();
    }
}

// ---------------------------------------------------------------------------
// Shared selectable-menu handler, used by both standalone and node mode.
// isNode picks the row-0 info line, the row-1 label, and the mode-switch
// action (row-2 "Boot HertzHunter" is the same in both):
//   node       : row0 "USB Node Mode" | row1 "Standalone"    -> bootStandalone()
//   standalone : row0 WiFi IP        | row1 "USB Node Mode" -> bootNodeMode()
// ---------------------------------------------------------------------------
void BoardDisplays::ncMenuProcess(bool isNode) {
    const char* switchLabel = isNode ? "Boot Standalone"    : "Boot USB Node Mode";
    const char* cfmMsg      = isNode ? "Start Standalone?" : "Start USB node?";
    String infoLine         = isNode ? String("USB Node Mode") : ncStandaloneInfoLine();

    // Refresh battery ~2 Hz (10-sample average, HHZ's updateBatteryVoltage).
    static unsigned long ncBatMs = 0;
    unsigned long nowMs = millis();
    if (nowMs - ncBatMs >= 500) {
        ncBattery.update();
        ncBatMs = nowMs;
    }

    // Buzzer on/off + battery alarm threshold come from HHZ's NVS settings
    // (configured in the Hertz Hunter menu). Cached: NVS open/close per call
    // would be wasteful at 1 kHz loop rate.
    static bool ncBuzOn = false;
    static int16_t ncAlarmMv = 3600;
    static bool ncCfgInit = false;
    if (!ncCfgInit) {
        ncBuzOn = HzSettings::buzzerEnabled();
        ncAlarmMv = HzSettings::batteryAlarmMv();
        ncCfgInit = true;
        ncBattery.update();
        Serial.printf("[NCD] battery: pin=%d raw=%lu mv=%d alarmMv=%d buzOn=%d\n",
                      BATTERY_PIN, (unsigned long)analogRead(BATTERY_PIN),
                      ncBattery.voltageMv(), (int)ncAlarmMv, (int)ncBuzOn);
    }

    bool prevEdge = ncButtonEdge(NC_PREV_BUTTON_PIN, nc_prevHi, nc_prevMs);
    bool nextEdge = ncButtonEdge(NC_NEXT_BUTTON_PIN, nc_nextHi, nc_nextMs);
    bool selEdge  = ncButtonEdge(MODE_SWITCH_PIN, nc_selHi, nc_selMs);

    // Beeps: single everywhere; double only on confirming a boot switch.
    if (ncBuzOn) {
        if (nc_confirm && selEdge) {
            ncBuzzer.doubleBuzz();
        } else if (prevEdge || nextEdge || selEdge) {
            ncBuzzer.buzz();
        }
    }

    // Low-battery alarm (HHZ behavior): voltage below the HHZ-configured
    // threshold for 1 s -> constant buzz until back above it.
    // Guard: a reading under 1.5V is a disconnected/bogus ADC, not a real
    // battery - never alarm on that (prevents a broken read from beeping
    // forever).
    static unsigned long ncLowBatMs = 0;
    if (ncBuzOn) {
        if (ncBattery.voltageMv() > 1500 && ncBattery.voltageMv() <= ncAlarmMv) {
            if (ncLowBatMs == 0) {
                ncLowBatMs = nowMs;
            } else if (nowMs - ncLowBatMs >= 1000) {
                ncBuzzer.startAlarm();
            }
        } else {
            ncLowBatMs = 0;
            ncBuzzer.stopAlarm();
        }
    }
#if NCD_DEBUG
    if (prevEdge || nextEdge || selEdge) {
        Serial.printf("[NCD] menu: prev=%d next=%d sel=%d row=%d cfm=%d\n",
                      prevEdge, nextEdge, selEdge, nc_sel, nc_confirm);
    }
#endif

    if (nc_confirm) {
        if (selEdge) {
            if (nc_confirm_hhz) {
                bootHertzHunter();   // does not return
            } else if (isNode) {
                bootStandalone();    // does not return
            } else {
                bootNodeMode();      // does not return
            }
        }
        if (prevEdge || nextEdge) {
            nc_confirm = false;      // cancel: back to the menu
        }
    } else {
        if (prevEdge || nextEdge) {
            nc_sel = !nc_sel;        // toggle the two selectable rows
        }
        if (selEdge) {
            nc_confirm_hhz = (nc_sel == 1);
            nc_confirm = true;
        }
    }

    // Redraw immediately on change (snappy); otherwise throttle to ~1 Hz.
    unsigned long now = millis();
    bool changed = (nc_sel != nc_lastSel) || (nc_confirm != nc_lastCfm);
    if (!changed && now - nc_last_redraw < 1000) {
        return;
    }
    nc_lastSel = nc_sel; nc_lastCfm = nc_confirm; nc_last_redraw = now;

    if (nc_confirm) {
        ncDrawMenu(infoLine, switchLabel, true, cfmMsg);
    } else {
        ncDrawMenu(infoLine, switchLabel, false, "");
    }
}

void BoardDisplays::initNuclearCounterNodeMode() {
    ncCommonInit();
    // Start on the first selectable row ("Boot Standalone"); confirm off.
    nc_sel = 0; nc_confirm = false; nc_confirm_hhz = false;
    nc_lastSel = -1; nc_lastCfm = false;   // force an immediate first draw
    ncBattery.update();                     // first voltage reading for display
    ncDrawMenu("USB Node Mode", "Boot Standalone", false, "");
}

void BoardDisplays::processNuclearCounterNode() {
    ncMenuProcess(true);
}

void BoardDisplays::processNuclearCounter(const String& ssid) {
    (void)ssid;
    ncMenuProcess(false);
}

void BoardDisplays::bootNodeMode() {
    // Enter USB node (RotorHazard) mode: set the flag and reboot.
    ncDisplay.clearBuffer();
    ncDisplay.setFont(u8g2_font_7x13B_tf);
    ncDisplay.drawStr(2, 12, "StarForgeOS");
    ncDisplay.setFont(u8g2_font_7x13_tf);
    ncDisplay.drawStr(2, 40, "Starting USB node");
    ncDisplay.drawStr(2, 54, "...");
    ncDisplay.sendBuffer();
    ConfigLoader::setBootNodeMode(true);   // persist across reboot
    delay(200);                            // let the screen flush over I2C
    esp_restart();
}

void BoardDisplays::bootStandalone() {
    // Return to standalone (timer) mode: clear the node-mode flag and reboot.
    ncDisplay.clearBuffer();
    ncDisplay.setFont(u8g2_font_7x13B_tf);
    ncDisplay.drawStr(2, 12, "StarForgeOS");
    ncDisplay.setFont(u8g2_font_7x13_tf);
    ncDisplay.drawStr(2, 40, "Booting Standalone");
    ncDisplay.drawStr(2, 54, "...");
    ncDisplay.sendBuffer();
    ConfigLoader::setBootNodeMode(false);
    delay(200);  // let the screen flush over I2C
    esp_restart();
}

#endif  // BOARD_NUCLEARCOUNTER
