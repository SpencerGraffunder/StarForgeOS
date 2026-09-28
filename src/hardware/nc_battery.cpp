#include "nc_battery.h"

NcBattery::NcBattery(uint8_t p, int16_t off)
    : pin(p), offsetMv(off), mv(0) {
    pinMode(pin, INPUT);
}

// Update internal voltage state. Accounts for the 2x voltage divider
// (identical math to Hertz Hunter Battery::updateBatteryVoltage).
//
// NOTE: uses raw 12-bit analogRead() + linear scaling instead of the
// framework's analogReadMilliVolts(): on this platform build (pioarduino
// framework 3.3.12) the ESP-IDF line-fit calibration path returns garbage
// (e.g. 42mV from a raw 2459/4095), which showed 0.0v and false-triggered
// the low-battery alarm. The ADC runs at 11dB attenuation (0-3.3V, set by
// TimingCore), so a direct linear map is correct and framework-independent.
void NcBattery::update() {
    int raw = 0;
    for (int i = 0; i < 10; i++) {
        raw += analogRead(pin);
    }
    raw /= 10;
    int pinMv = raw * 3300 / 4095;
    // HHZ math: battery = pin voltage x 2 (voltage divider).
    // (HHZ stores round(pinMv/100*2) in 0.1V units == pinMv*2 in mV.)
    mv = static_cast<int16_t>(pinMv * 2 + offsetMv);
}

int16_t NcBattery::voltageMv() const {
    return mv;
}
