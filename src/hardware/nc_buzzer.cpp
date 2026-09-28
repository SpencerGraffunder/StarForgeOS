#include "nc_buzzer.h"

NcBuzzer::NcBuzzer(uint8_t p)
    : pin(p), alarmHandle(NULL) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

// Single buzz, spawned on a task to prevent blocking.
void NcBuzzer::buzz() {
    xTaskCreate(buzzTask, "nc_buzz", NC_BUZZ_STACK_SIZE, this, 1, NULL);
}

// Double buzz.
void NcBuzzer::doubleBuzz() {
    xTaskCreate(doubleBuzzTask, "nc_dblbuzz", NC_BUZZ_STACK_SIZE, this, 1, NULL);
}

void NcBuzzer::buzzTask(void* parameter) {
    NcBuzzer* b = static_cast<NcBuzzer*>(parameter);
    digitalWrite(b->pin, HIGH);
    vTaskDelay(pdMS_TO_TICKS(NC_BUZZ_DURATION_MS));
    digitalWrite(b->pin, LOW);
    vTaskDelete(NULL);
}

void NcBuzzer::doubleBuzzTask(void* parameter) {
    NcBuzzer* b = static_cast<NcBuzzer*>(parameter);

    digitalWrite(b->pin, HIGH);
    vTaskDelay(pdMS_TO_TICKS(NC_BUZZ_DURATION_MS));
    digitalWrite(b->pin, LOW);

    vTaskDelay(pdMS_TO_TICKS(NC_BUZZ_DELAY_MS));

    digitalWrite(b->pin, HIGH);
    vTaskDelay(pdMS_TO_TICKS(NC_BUZZ_DURATION_MS));
    digitalWrite(b->pin, LOW);

    vTaskDelete(NULL);
}

// Start constant buzzing alarm (HHZ Buzzer::startAlarm).
void NcBuzzer::startAlarm() {
    if (alarmHandle == NULL) {
        xTaskCreate(alarmTask, "nc_alarm", NC_BUZZ_STACK_SIZE, this, 1, &alarmHandle);
    }
}

// Stop constant buzzing alarm (HHZ Buzzer::stopAlarm).
void NcBuzzer::stopAlarm() {
    if (alarmHandle != NULL) {
        vTaskDelete(alarmHandle);
        alarmHandle = NULL;
        digitalWrite(pin, LOW);  // pin may have been left high mid-pulse
    }
}

void NcBuzzer::alarmTask(void* parameter) {
    NcBuzzer* b = static_cast<NcBuzzer*>(parameter);
    while (1) {
        digitalWrite(b->pin, HIGH);
        vTaskDelay(pdMS_TO_TICKS(NC_BUZZ_DURATION_MS));
        digitalWrite(b->pin, LOW);
        vTaskDelay(pdMS_TO_TICKS(NC_BUZZ_DELAY_MS));
    }
}
