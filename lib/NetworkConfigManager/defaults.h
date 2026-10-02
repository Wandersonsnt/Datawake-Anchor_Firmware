#ifndef DEFAULTS_H
#define DEFAULTS_H

#define DEFAULT_NVS_NAMESPACE "wk_config"

//Ethernet:
#define DEFAULT_USE_ETHERNET true
#define DEFAULT_ETH_DHCP true
#define DEFAULT_ETH_IP "192.168.1.100"
#define DEFAULT_ETH_MASK "255.255.255.0"
#define DEFAULT_ETH_GW "192.168.1.1"

//WiFi:
#define DEFAULT_USE_WIFI true
#define DEFAULT_WIFI_SSID "CFP116_ADM"
#define DEFAULT_WIFI_PASSWORD "ac1ce1ss6"
#define DEFAULT_WIFI_DHCP true
#define DEFAULT_WIFI_IP "192.168.1.100"
#define DEFAULT_WIFI_MASK "255.255.255.0"
#define DEFAULT_WIFI_GW "192.168.1.1"

//Broker MQTT:
#define DEFAULT_BROKER_PATH "wkmqtt.brazilsouth-1.ts.eventgrid.azure.net"
#define DEFAULT_BROKER_PORT 8883
#define DEFAULT_BROKER_USER "subscriber-authnID"
#define DEFAULT_BROKER_PASSWORD ""
#define DEFAULT_BROKER_TOPIC_WRITE "localizacao/teste"
#define DEFAULT_BROKER_TOPIC_READ "localizacao/teste/CMD"
#define DEFAULT_BROKER_USE_TLS true
#define DEFAULT_BROKER_CERT_FROM_FACTORY true

#endif