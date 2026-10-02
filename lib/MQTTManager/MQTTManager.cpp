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
char* mqtt_user;    // Nome de usuário
char* mqtt_pass;    // Senha do usuário

// Variável para armazenar o ID do cliente único (MacAddress)
String client_id;
String topicToSubscribe;

/* @brief Função para realizar tentativa de reconexão ao MQTT */
bool reconnectMQTT() {
  log_i("Conectando MQTT... ");
  if (client.connect(client_id.c_str(), mqtt_user, mqtt_pass)) {
    log_i("Conectado ao MQTT com sucesso!");
    client.subscribe(topicToSubscribe.c_str());
    log_i("Subscrito no tópico: %s.", topicToSubscribe.c_str());
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

/* @brief Função para configurar o gerenciador MQTT 
 * @param cb Função de callback para tratar mensagens recebidas (com parametros: char* topic, byte* payload, unsigned int length)
 * @param subscribeTopic Tópico para se subscrever
 * @return true se a configuração for bem-sucedida, false caso contrário
 */
bool setupMQTTManager(MqttCallback cb, String subscribeTopic, String brokerAddress, String brokerUser, String brokerPass, int brokerPort, bool useTLS, bool useFactoryKeys, String caCert, String clientCert, String clientKey) {
  userCallback = cb; // Armazena a função de callback
  topicToSubscribe = subscribeTopic; // Armazena globalmente o tópico para subscrever

  char buffer[brokerUser.length() + 1];
  brokerUser.toCharArray(buffer, sizeof(buffer));
  mqtt_user = buffer; // Armazena globalmente o nome de usuário

  char bufferPass[brokerPass.length() + 1];
  brokerPass.toCharArray(bufferPass, sizeof(bufferPass));
  mqtt_pass = bufferPass; // Armazena globalmente a senha do usuário

  // Gera um ID de cliente único baseado no MacAddress do ESP32 para evitar conflitos de conexão
  uint8_t macAddress[6];
  esp_read_mac(macAddress, ESP_MAC_WIFI_STA);
  char macStr[12] = {0};
  sprintf(macStr, "%02X%02X%02X%02X%02X%02X", macAddress[0], macAddress[1], macAddress[2], macAddress[3], macAddress[4], macAddress[5]);
  client_id = "Anc_" + String(macStr);
    
  // Configura a segurança TLS (mTLS)
  if (useTLS) {
    client.setClient(espClientSecure);
    if (useFactoryKeys) {
      log_i("Usando certificados de fábrica para TLS...");
      espClientSecure.setCACert(root_ca); // Usa o certificado CA de fábrica
      espClientSecure.setCertificate(client_cert); // Usa o certificado do cliente de fábrica
      espClientSecure.setPrivateKey(client_key); // Usa a chave privada do cliente de fábrica
    } else {
      log_i("Usando certificados personalizados para TLS...");
      espClientSecure.setCACert(caCert.c_str()); // Usa o certificado CA fornecido pelo usuário
      espClientSecure.setCertificate(clientCert.c_str()); // Usa o certificado do cliente fornecido pelo usuário
      espClientSecure.setPrivateKey(clientKey.c_str()); // Usa a chave privada do cliente fornecida pelo usuário
    }
  } else {
    log_w("TLS desabilitado! Conexão MQTT sem segurança.");
    client.setClient(espClientPlain);
    // espClientSecure.setInsecure(); // Ignora a checagem do Root CA
  }

  // Aumenta o buffer para lidar com os pacotes robustos (Necessário para a Azure)
  client.setBufferSize(MQTT_BUFFER_SIZE);

  // Configura o servidor e o callback interno
  client.setServer(brokerAddress.c_str(), brokerPort);
  client.setCallback(internalCallback);

  log_i("Conectando ao broker MQTT em %s:%d com usuário '%s'...", brokerAddress.c_str(), brokerPort, brokerUser.c_str());
  
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
  if (!client.connected()) { // Se não estiver conectado, tenta reconectar
    static long lastReconnectAttempt = 0;
    long now = millis();
    if (now - lastReconnectAttempt > MQTT_RECONNECT_INTERVAL) { // Tenta reconectar em intervalos mais longos para evitar sobrecarga
      lastReconnectAttempt = now;
      if (reconnectMQTT()){
        lastReconnectAttempt = 0;
      }
    }
  } else {
    client.loop(); // Processa mensagens internas e keep-alive
  }
}

/* @brief Função para verificar o estado de conexão do MQTT
 * @return true se o cliente MQTT estiver conectado, false caso contrário
 */
bool isMQTTConnected() {
  return client.connected();
}