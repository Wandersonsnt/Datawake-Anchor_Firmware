#ifndef BATTERY_PROCESSOR_H
#define BATTERY_PROCESSOR_H

#include <Arduino.h>
#include "ProjectConfig.h"

// Definição da assinatura da função de Callback
typedef void (*BatteryCallback)(String tag_id, uint8_t batteryLevel);

// Funções públicas
void processNewBatteryReading(String tag_id, uint8_t batteryLevel);
void setBatteryProcessorCallback(BatteryCallback cb);
#endif