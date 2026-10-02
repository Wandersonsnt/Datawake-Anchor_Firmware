#include <Arduino.h>
#include "NetworkConfigManager.h"

NetworkConfigManager configManager;

void WifiChangedToConnected() { // Callback chamado quando o Wi-Fi se conecta
    Serial.println("[MAIN] Wi-Fi reconectado.");
    //Apaga o LED vermelho.
}

void WifiChangedToDisconnected() { // Callback chamado quando o Wi-Fi se desconecta
    Serial.println("[MAIN] Wi-Fi desconectado.");
    //Liga o LED vermelho.
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("Iniciando WK-Anchor Firmware...");

    // Registra os callbacks para alertar sobre mudanças de rede:
    configManager.setOnWifiConnectCallback(WifiChangedToConnected);
    configManager.setOnWifiDisconnectCallback(WifiChangedToDisconnected);
    //configManager.setOnMQTTConfigChangedCallback(MqttConfigChanged); // Caso seja necessário tratar a mudança de configuração do Broker MQTT em tempo de execução, registre este callback.
    
    // Inicia o gerenciador de rede:
    configManager.begin();
}

void loop() {
    configManager.loop(); // Mantém o servidor web ativo e monitora o status do Wi-Fi.

    if (configManager.isWiFiConnected()) {
        // Lógica para conectar ao MQTT e processar dados...
    }

}