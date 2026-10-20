#include "MQTTManager.h"
#include "MQTTSecrets.h"
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// Variáveis internas privadas
WiFiClient espClientPlain;
WiFiClientSecure espClientSecure;
// PubSubClient client(espClientSecure);
PubSubClient client;
MqttCallback userCallback = nullptr;

// Variáveis para armazenar dados de conexão com o oBroker MQTT
char mqtt_user[128];    // Nome de usuário
char mqtt_pass[128];    // Senha do usuário
char mqtt_domain[128];  // Domínio do broker
int mqtt_port;      // Porta do broker
char client_id[17]; // ID único do cliente MQTT
char topicToSubscribe[128]; // Tópico para subscrever
char cert_ca[5121];  // Certificado da CA
char cert_client[5121]; // Certificado do cliente
char key_client[5121];  // Chave privada do cliente
bool use_tls = false; // Flag para indicar se a conexão deve usar TLS

/* @brief Função para realizar tentativa de reconexão ao MQTT */
bool reconnectMQTT() {
  log_i("Tentando conectar ao MQTT com client_ID: '%s'...", client_id);

  if (client.connect(client_id, mqtt_user, mqtt_pass)) {
    log_i("Conectado ao MQTT com sucesso!");
    client.subscribe(topicToSubscribe);
    log_i("Subscrito no tópico: %s.", topicToSubscribe);
  } else {
    log_e("Falha na conexão MQTT, rc=%d", client.state());
  }
  return client.connected();
}

/* @brief Função para desabilitar o MQTT */
bool disableMQTT() {
  client.disconnect();
  return true;
}

/* @brief Wrapper interno que chama o callback externo do main.cpp */
void internalCallback(char* topic, byte* payload, unsigned int length) {
  if (userCallback != nullptr) {
    userCallback(topic, payload, length);
  }
}

/* @brief Função configurar o MQTT */
void configureMQTT() {
  // Configuração da segurança TLS (mTLS)
  if (use_tls) {
    client.setClient(espClientSecure);
   
    // Configura os certificados e chaves para a conexão segura
    espClientSecure.setCACert(cert_ca);
    espClientSecure.setCertificate(cert_client);
    espClientSecure.setPrivateKey(key_client);
   
  } else {
    client.setClient(espClientPlain);
  }
  
  client.setBufferSize(MQTT_BUFFER_SIZE); // Aumenta o buffer para lidar com os pacotes robustos (Necessário para a Azure)
  client.setCallback(internalCallback); // Configura o callback interno
  client.setServer(mqtt_domain, mqtt_port); // Configura o servidor

  log_i("Configurado MQTT com TLS %s no domínio %s:%d", use_tls ? "habilitado" : "desabilitado", mqtt_domain, mqtt_port);
}

/* @brief Função para configurar o gerenciador MQTT 
 * @param cb Função de callback para tratar mensagens recebidas (com parametros: char* topic, byte* payload, unsigned int length)
 * @param subscribeTopic Tópico para se subscrever
 * @return true se a configuração for bem-sucedida, false caso contrário
 */
bool setupMQTTManager(MqttCallback cb, String subscribeTopic, String brokerAddress, String brokerUser, String brokerPass, int brokerPort, bool useTLS, bool useFactoryKeys, String caCert, String clientCert, String clientKey) {
  userCallback = cb; // Armazena a função de callback

  if (mqtt_port < 0) mqtt_port = 0; // Garante que a porta seja não-negativa
  if (mqtt_port > 65535) mqtt_port = 65535; // Garante que a porta seja no máximo 65535
  mqtt_port = brokerPort; // Armazena globalmente a porta do broker

  // char buffer[brokerUser.length() + 1];
  // brokerUser.toCharArray(buffer, sizeof(buffer));
  // mqtt_user = buffer; // Armazena globalmente o nome de usuário
  strlcpy(mqtt_user, brokerUser.c_str(), sizeof(mqtt_user)); // Armazena globalmente o nome de usuário

  // char bufferPass[brokerPass.length() + 1];
  // brokerPass.toCharArray(bufferPass, sizeof(bufferPass));
  // mqtt_pass = bufferPass; // Armazena globalmente a senha do usuário
  strlcpy(mqtt_pass, brokerPass.c_str(), sizeof(mqtt_pass)); // Armazena globalmente a senha do usuário

  // char bufferDomain[brokerAddress.length() + 1];
  // brokerAddress.toCharArray(bufferDomain, sizeof(bufferDomain));
  // mqtt_domain = bufferDomain; // Armazena globalmente o domínio do broker
  strlcpy(mqtt_domain, brokerAddress.c_str(), sizeof(mqtt_domain)); // Armazena globalmente o domínio do broker

  // char bufferTopic[subscribeTopic.length() + 1];
  // subscribeTopic.toCharArray(bufferTopic, sizeof(bufferTopic));
  // topicToSubscribe = bufferTopic; // Armazena globalmente o tópico para subscrever
  strlcpy(topicToSubscribe, subscribeTopic.c_str(), sizeof(topicToSubscribe)); // Armazena globalmente o tópico para subscrever

  // Gera um ID de cliente único baseado no MacAddress do ESP32 para evitar conflitos de conexão
  uint8_t macAddress[6];
  esp_read_mac(macAddress, ESP_MAC_WIFI_STA);
  sprintf(client_id, "Anc_%02X%02X%02X%02X%02X%02X\0", macAddress[0], macAddress[1], macAddress[2], macAddress[3], macAddress[4], macAddress[5]); // Armazena globalmente o ID do cliente MQTT

  use_tls = useTLS; // Armazena globalmente a flag de uso de TLS

  //Copia dos certificados e chaves para as variáveis globais
  if (useTLS) {
    if (useFactoryKeys) {
      strlcpy(cert_ca, factory_caCert, sizeof(cert_ca));
      strlcpy(cert_client, factory_clientCert, sizeof(cert_client));
      strlcpy(key_client, factory_clientKey, sizeof(key_client));
    } else {
      strlcpy(cert_ca, caCert.c_str(), sizeof(cert_ca));
      strlcpy(cert_client, clientCert.c_str(), sizeof(cert_client));
      strlcpy(key_client, clientKey.c_str(), sizeof(key_client));
    }
  }

  configureMQTT(); // Configura o gerenciador MQTT com os dados fornecidas
    
  // Tenta a primeira conexão e retorna o resultado
  return reconnectMQTT();
}

/* @brief Função para publicar uma mensagem no tópico MQTT especificado 
 * @param topic Tópico para publicar a mensagem
 * @param message Mensagem a ser publicada
 * @return true se a publicação for bem-sucedida, false caso contrário
 */
bool publishMQTT(const char* topic, const char* message) {
  if (!client.connected()) {
    return false;
  }
  return client.publish(topic, message);
}

/* @brief Função para atualizar o estado do gerenciador MQTT. Deve ser chamada periodicamente no loop principal para manter a conexão e processar mensagens */
void loopMQTTManager() {
  static long lastReconnectAttempt = 0; // Armazena o timestamp da última tentativa de reconexão
  static int consecutiveFailures = 0; // Contador de falhas consecutivas para reiniciar a pilha MQTT se necessário

  if (!client.connected()) { // Se não estiver conectado, tenta reconectar
    long now = millis();
    if (now - lastReconnectAttempt > MQTT_RECONNECT_INTERVAL) { // Tenta reconectar em intervalos mais longos para evitar sobrecarga
      lastReconnectAttempt = now;
      consecutiveFailures++;
      log_w("Tentativa de reconexão MQTT #%d...", consecutiveFailures);
      if (reconnectMQTT()){
        lastReconnectAttempt = 0;// Reseta o timestamp da última tentativa de reconexão para que a próxima tentativa ocorra instantaneamente se a conexão cair novamente
        consecutiveFailures = 0; // Reseta o contador de falhas consecutivas
      } else if (consecutiveFailures == 5){
        log_e("ERRO: Falha na reconexão MQTT. Tentando reconfigurar...");
        disableMQTT(); // Desconecta o cliente MQTT antes de reconfigurar
        configureMQTT(); // Reconfigura o MQTT para tentar corrigir problemas de configuração
        reconnectMQTT(); // Tenta reconectar novamente após reconfiguração
      } else if (consecutiveFailures >= 10) {
        log_e("ERRO CRITICO: Falha na reconexão MQTT. Forçando reinício do sistema!");
        client.disconnect(); // Desconecta fisicamente as interfaces para limpar os sockets corrompidos
        delay(1000);
        ESP.restart(); // Reinicia o microcontrolador para tentar recuperar de um estado crítico de falha na conexão MQTT
      }
    }
  } else {
    client.loop(); // Processa mensagens internas e keep-alive

    // Garante que as variáveis de reconexão sejam zeradas mesmo quando a conexão aconteceu em outro processo (como no setup).
    lastReconnectAttempt = 0;
    consecutiveFailures = 0; 
  }
}

/* @brief Função para verificar o estado de conexão do MQTT
 * @return true se o cliente MQTT estiver conectado, false caso contrário
 */
bool isMQTTConnected() {
  return client.connected();
}