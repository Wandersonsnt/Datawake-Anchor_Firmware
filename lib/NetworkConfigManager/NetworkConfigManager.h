#ifndef ANCHOR_CONFIG_MANAGER_H
#define ANCHOR_CONFIG_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ETH.h>
#include <SPI.h>
#include <WebServer.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <FS.h>
#include <DNSServer.h> // Necessário para o captive portal (no modo Access Point)
#include <time.h>
#include <esp_mac.h> // Necessário para obter o endereço MAC do ESP32 antes de iniciar o AP para gerar credenciais únicas
#include <Update.h> // Necessário para o OTA funcionar corretamente
#include <defaults.h>



class NetworkConfigManager {
public:
    NetworkConfigManager();

    String FwVersion = "v1.0.0"; // Versionamento do firmware e apresentação no WebServer de configuração

    // Definição dos pinos do W5500 na placa:
    #define W5500_MISO 37
    #define W5500_MOSI 35
    #define W5500_SCK  36
    #define W5500_CS   39
    #define W5500_INT  42
    #define W5500_RST  41

    // Tipos de conexão ativas
    enum ActiveNet { NET_NONE, NET_WIFI, NET_ETH };   

    // Definições dos tipos de ponteiros e métodos para registrar os callbacks no main.cpp
    typedef void (*NetworkChangeCallback)(bool connected, String interfaceType);
    typedef void (*MQTTCallback)();
    void setOnNetworkChangeCallback(NetworkChangeCallback callback);
    void setOnMQTTConfigChangedCallback(MQTTCallback callback);

    void begin();
    void loop();
    bool isWiFiConnected();
    void clearConfig();
    void changeMqttState(bool connected);
    void setupTime();
    time_t getCurrentTimestamp();

    // Métodos para obter configurações no main.cpp
    String getBrokerAddress();
    int getBrokerPort();
    String getBrokerUser();
    String getBrokerPass();
    String getBrokerTopicWrite();
    String getBrokerTopicRead();
    bool getUseTLS();
    bool getCertType();
    String getCACert();
    String getClientCert();
    String getClientKey();
    String getLocalIP();
    String getMacAddress();
    byte getCurrentConnection();

    // Rotas de controle OTA
    void handleOTAUpdate();
    void handleOTAUpload();

private:
    WebServer server;
    Preferences preferences;

    // Variáveis de controle de estado
    bool ethConnected = false;
    bool wifiConnected = false;
    ActiveNet activeInterface = NET_NONE;
    
    // Tratador de eventos de rede
    void handleNetworkEvent(arduino_event_id_t event, arduino_event_info_t info);
    
    // Ponteiros para funções de callback
    NetworkChangeCallback onNetworkChangeCallback = nullptr;
    MQTTCallback onMQTTConfigChangedCallback = nullptr;

    // Ponteiro para o File System e variável auxiliar para salvar o arquivo em partes
    fs::FS* _fs;
    File uploadFile;
    
    // Credenciais AP (Ponto de Acesso da Âncora):
    String apSSID;
    String apPassword;
    
    // Configurações de rede Ethernet e Wi-Fi:
    uint8_t macAddress[6];
    String macAddressStr = "";
    bool useEthernet;
    bool useWiFi;
    String staSSID;
    String staPassword;
    bool ethDhcp;
    String ethIp;
    String ethMask;
    String ethGw;
    bool wifiDhcp;
    String wifiIp;
    String wifiMask;
    String wifiGw;

    // Variáveis para o Captive Portal
    DNSServer dnsServer;
    const byte DNS_PORT = 53;
    void handleNotFound();
    
    // Dados do Broker MQTT:
    String brokerAddress;
    String brokerUser;
    String brokerPass;
    int brokerPort;
    String brokerTopicWrite;
    String brokerTopicRead;
    bool brokerUseTLS;
    bool brokerCertFromFactory;

    bool lastWiFiState = false;
    bool lastMQTTState;

    void setupAP(bool hidden);
    void loadConfig();
    void handleMqttUpload();
    
    // Handlers do WebServer
    void handleRoot();
    void handleScanWiFi();
    void handleSaveNetwork();
    void handleSaveMQTT();
    void handleRestart();
    void handleFactoryReset();
    void handleCertRequest();
};

#endif