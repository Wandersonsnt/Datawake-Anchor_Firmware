#ifndef LOCATION_PROCESSOR_H
#define LOCATION_PROCESSOR_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "ProjectConfig.h" // Configurações do projeto

// Definição da assinatura da função de Callback
typedef void (*LocationCallback)(String tag_id, float azimuth, float elevation, float rssi, float min_azimuth, float max_azimuth, float min_elevation, float max_elevation, uint8_t samples);

// Funções públicas
void setupLocationProcessor();
void configLocationProcessor(uint16_t ReadingsToPublish = 10, uint16_t MinIntervalToSend = 1000, uint16_t SlotTimeoutMs = 5000);
void setLocationProcessorCallback(LocationCallback cb);
void processNewLocationReading(String id, float raw_azimuth, float raw_elevation, float rssi);

#endif