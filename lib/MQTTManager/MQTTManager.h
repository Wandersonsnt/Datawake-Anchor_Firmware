#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <esp_mac.h>

#define MQTT_BUFFER_SIZE 1024 // Tamanho do buffer para mensagens MQTT
#define MQTT_RECONNECT_INTERVAL 5000 // Intervalo de reconexão em milissegundos

// Definição do tipo para a função de callback que o usuário registrará.
typedef void (*MqttCallback)(char* topic, byte* payload, unsigned int length);

// Funções públicas da biblioteca
bool setupMQTTManager(MqttCallback cb, String subscribeTopic, String brokerAddress, String brokerUser, String brokerPass, int brokerPort, bool useTLS = true, bool useFactoryKeys = true, String caCert = "", String clientCert = "", String clientKey = "");
void loopMQTTManager();
bool publishMQTT(const char* topic, const char* message);
bool reconnectMQTT();
bool disableMQTT();
bool isMQTTConnected();

#endif