#ifndef NC_BUZZER_H
#define NC_BUZZER_H

#include <Arduino.h>

// Same timing as the NuclearCounter firmware (Buzzer class): 20ms pulse,
// 80ms between pulses. Beeps run on FreeRTOS tasks so they never block
// the main loop.
#define NC_BUZZ_DURATION_MS  20
#define NC_BUZZ_DELAY_MS     80
#define NC_BUZZ_STACK_SIZE   2048

class NcBuzzer {
public:
    explicit NcBuzzer(uint8_t pin);
    void buzz();        // single 20ms pulse (NuclearCounter: button navigation)
    void doubleBuzz();  // two pulses, 80ms apart (NuclearCounter: confirm boot switch)
    void startAlarm();  // constant buzz (NuclearCounter: low battery) until stopAlarm()
    void stopAlarm();

private:
    static void buzzTask(void* parameter);
    static void doubleBuzzTask(void* parameter);
    static void alarmTask(void* parameter);
    uint8_t pin;
    TaskHandle_t alarmHandle;
};

#endif // NC_BUZZER_H
