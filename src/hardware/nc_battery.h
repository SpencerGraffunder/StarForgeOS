#ifndef NC_BATTERY_H
#define NC_BATTERY_H

#include <Arduino.h>

// Battery voltage reader for the NuclearCounter board, ported from the
// Hertz Hunter firmware (Battery class): same ADC pin, same 2x voltage
// divider scaling, same 10-sample averaging. Returns millivolts.
class NcBattery {
public:
    explicit NcBattery(uint8_t pin, int16_t offsetMv);

    void update();                // refresh the reading (10-sample average)
    int16_t voltageMv() const;   // last reading in millivolts

private:
    uint8_t pin;
    int16_t offsetMv;
    int16_t mv;
};

#endif // NC_BATTERY_H
