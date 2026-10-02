#include <Arduino.h>
#include <HardwareSerial.h>
#include "driver/uart.h"
#include "LocationProcessor.h" // Módulo que processa os dados recebidos, realiza filtro e gerencia o envio via MQTT
#include "BatteryProcessor.h"
#include "MQTTManager.h" // Biblioteca para gerenciar conexões e publicações MQTT
#include "NetworkConfigManager.h" // Biblioteca para gerenciar a configuração de rede via webserver
#include "ProjectConfig.h" // Configurações do projeto (constantes, pinos, etc)

#define FIRMWARE_VERSION "1.0.4"

NetworkConfigManager configManager; // Objeto para gerenciar a configuração de rede via webserver


//Variáveis do servidor NTP para sincronização do relógio interno do ESP32, necessário para envio do timestamp para o broker
time_t now; 

bool isMqttRunning = false;       // Flag para indicar se o MQTT foi iniciado
bool connectedToNetwork = false;  // Flag para indicar conexão com a rede

// UART2 para comunicação com a PCB de array de antenas da âncora
HardwareSerial AnchorSerial(2);
#define UART_BUFFER_SIZE 256
char uartBuffer[UART_BUFFER_SIZE];
int uartIndex = 0;

char anchor_id[32];
bool useRawAngles = false; // Variável para controlar se os ângulos brutos (raw) devem ser usados ou não


//---LED Functions---
/* @brief Função para controlar o LED vermelho da âncora via comando AT
 * @param state Estado do LED vermelho (1 para acender, 0 para apagar, 2 para alterar o estado atual)
 */ 
void anchorRedLed(byte state){
  static bool redLed = false;
  if (redLed != state) {
    redLed = !redLed;
    String payload;
    payload += "AT+ULED=1," + String(redLed) + "\r";
    AnchorSerial.write(payload.c_str());
  }
}

/* @brief Função para controlar o LED verde da âncora via comando AT
 * @param state Estado do LED verde (1 para acender, 0 para apagar, 2 para alterar o estado atual)
 */ 
void anchorGreenLed(byte state){
  static bool greenLed = false;
  if (greenLed != state) {
    greenLed = !greenLed;
    String payload;
    payload += "AT+ULED=2," + String(greenLed) + "\r";
    AnchorSerial.write(payload.c_str());
  }
}

/* @brief Função para controlar o LED azul da âncora via comando AT
 * @param state Estado do LED azul (1 para acender, 0 para apagar, 2 para alterar o estado atual)
 */ 
void anchorBlueLed(byte state){
  static bool blueLed = false;
  if (blueLed != state) {
    blueLed = !blueLed;
    String payload;
    payload += "AT+ULED=3," + String(blueLed) + "\r";
    AnchorSerial.write(payload.c_str());
  }
}

/* @brief Função para configurar a âncora U-Blox com os parâmetros */
void configureUbloxBoard(){
  log_d("Configurando a âncora U-Blox...");

  // Configura a âncora para enviar apenas os dados necessários (UUDF e UUDFP)
  AnchorSerial.write("\r"); // Limpa o buffer caso existiam dados antigos, garantindo que os próximos parâmetros serão aceitos
  delay(500);
  AnchorSerial.write("AT+UDFCFG=5,1\r");    // Habilita o envio de dois angulos
  delay(500);
  AnchorSerial.write("AT+UDFCFG=10,20\r");  // Fator para suavização de resultados (0-99). Quanto maior, mais suave será a leitura, porém mais lenta a movimentação.
  delay(500);
  AnchorSerial.write("AT+UDFCFG=121,1\r");  // Habilita o envio de azimuth_raw e elevation_raw (valores brutos sem suavização)
  delay(500);
  AnchorSerial.write("AT+USYSCFG=1,0\r");   //Desabilita a funcionalidade padrão dos LEDs da antena
  delay(500);

  log_d("Configuração da âncora concluída.");
}

/* @brief Função para enviar mensagens via MQTT */
void mqttSendModule(const char *message){
  if (publishMQTT(configManager.getBrokerTopicWrite().c_str(), message)){
    log_i("Publicado: %s --> %s\n", configManager.getBrokerTopicWrite().c_str(), message);
    anchorRedLed(false); //Apaga o LED vermelho, caso tenha sido acendido anteriormente
    anchorBlueLed(2); // Altera o status do led azul
  } else {
    log_e("Falha ao publicar no MQTT.\n");
    anchorRedLed(true); // Acende o LED vermelho para indicar falha no envio
  }
}

//---CALLBACK Functions---
/* @brief Função callback chamada quando a conexão de rede muda
 * @param isConnected Indica se a rede está conectada (true) ou desconectada (false)
 * @param interfaceType Tipo da interface de rede ("WIFI" ou "ETH")
 */
void networkChanged(bool isConnected, String interfaceType) {
  if (isConnected) {
    log_w("Rede operando via %s. Inicializando MQTT...\n", interfaceType.c_str());
    connectedToNetwork = true;
  } else {
    log_e("Sem conexão de rede! Pausando envios U-Blox...");
    connectedToNetwork = false;
  }
}

/* @brief Função callback chamada quando uma mensagem é recebida via MQTT
 * @param topic Tópico da mensagem recebida
 * @param payload Conteúdo da mensagem recebida
 * @param length Comprimento do conteúdo da mensagem
 */
void anchorMqttCallback(char *topic, byte *payload, unsigned int length){
  String msg;
  for (int i = 0; i < length; i++){
    msg += (char)payload[i];
  }
  log_i("Recebido via MQTT: %s\r\n", msg.c_str());

  // Extrai o conteúdo do formato JSON:
  int typeValue = -1;
  String contentValue = "";
  int typeKeyIdx = msg.indexOf("\"type\"");

  if (typeKeyIdx != -1) {
    int colonIdx = msg.indexOf(':', typeKeyIdx);
    if (colonIdx != -1) {
      typeValue = msg.substring(colonIdx + 1).toInt();
    }
  }

  int contentKeyIdx = msg.indexOf("\"content\"");
  if (contentKeyIdx != -1) {
    int colonIdx = msg.indexOf(':', contentKeyIdx); // Encontra os dois pontos após o "content"
    if (colonIdx != -1) {
      int startQuote = msg.indexOf('"', colonIdx); // Encontra as aspas de abertura do valor
      if (startQuote != -1) {
        int endQuote = msg.indexOf('"', startQuote + 1); // Encontra as aspas de fechamento
        if (endQuote != -1) {
          contentValue = msg.substring(startQuote + 1, endQuote); // Extrai apenas o que está dentro das aspas
        }
      }
    }
  }

  // --- COMANDOS PARA O ESP32 --- EX: {"type":10,"content":"RESTART"}
  if (typeValue == 10) { 
    log_i("Comando local recebido: %s", contentValue.c_str());
    
    if (contentValue == "RESTART") {
      log_i("Reiniciando o ESP32 a pedido do servidor...");
      delay(1000);
      ESP.restart();
    } 
    else if (contentValue == "UseRawAngles=1") {
      useRawAngles = true;
      log_i("Configuração alterada: Usando ângulos brutos (raw) para cálculos.");
    } 
    else if (contentValue == "UseRawAngles=0") {
      useRawAngles = false;
      log_i("Configuração alterada: Usando ângulos filtrados para cálculos.");
    } 
    else if (contentValue == "?") {
      log_i("Sending anchor data to MQTT...");

      // Payload
      char payload[256];

      snprintf(payload, sizeof(payload),
              "{"
              "\"type\":12,"
              "\"anchorMacAddress\":\"%s\","
              "\"anchorIP\":\"%s\","
              "\"connectionType\":\"%s\","
              "\"timestamp_sec\":%ld"
              "}",
              configManager.getMacAddress().c_str(),
              configManager.getLocalIP().c_str(),
              configManager.getCurrentConnection()==1 ? "Wi-Fi" : "Ethernet",
              configManager.getCurrentTimestamp()
              );

      mqttSendModule(payload); // Publica no tópico de escrita

    } else if (contentValue == "FACTORY") { 
      log_w("Aplicando configurações de fábrica...");
      configManager.clearConfig();
      configureUbloxBoard();
      delay(1000);
      ESP.restart();
    } else {
      log_e("O comando recebido não corresponde a lista de funções do dispositivo.");
    }
  } 

  // --- COMANDOS PARA A ANT-B10 (U-BLOX) --- EX: {"type":11,"content":"AT+UDFCFG=4"}
  else if (typeValue == 11) {
    log_i("Comando encaminhado para U-BLOX: %s", contentValue.c_str());
    AnchorSerial.print(contentValue + "\r"); //"\r\n"
  } 

  // Tipo não reconhecido ou erro de formatação
  else {
    log_e("Formato JSON invalido ou tipo desconhecido descartado.");
  }
}

/* @brief Função de callback para enviar dados de localização via MQTT quando o algoritmo de cálculo é finalizado
 * @param tag_id ID da tag
 * @param azimuth Azimute da tag
 * @param elevation Elevação da tag
 * @param rssi Intensidade do sinal recebido (RSSI)
 */
void sendLocationToBroker(String tag_id, float azimuth, float elevation, float rssi, float min_azimuth, float max_azimuth, float min_elevation, float max_elevation, uint8_t samples) {
  // Preparação do Payload
  char payload[256];
  snprintf(payload, sizeof(payload),
          "{"
          "\"type\":1,"
          "\"anchorMacAddress\":\"%s\","
          "\"assetMacAddress\":\"%s\","
          "\"rssi\":%s,"
          "\"azimuth\":% .1f,"
          "\"elevation\":% .1f,"
           "\"min_azimuth\":% .1f,"
           "\"max_azimuth\":% .1f,"
           "\"min_elevation\":% .1f,"
           "\"max_elevation\":% .1f,"
          "\"samples\":%d,"
          "\"timestamp_sec\":%ld"
          "}",
          configManager.getMacAddress().c_str(),
          tag_id,
          String(rssi).c_str(),
          azimuth,
          elevation,
           min_azimuth,
           max_azimuth,
           min_elevation,
           max_elevation,
          samples,
          configManager.getCurrentTimestamp()
          );

  mqttSendModule(payload); // Publica no tópico de escrita
}

/* @brief Função de callback para enviar dados de nível de bateria das tags 
 * @param tag_id ID da tag
 * @param battery_level Nível de bateria reportado (de 0 a 100)
 */
void sendBatteryToBroker(String tag_id, uint8_t battery_level){
  char payloadToSend[256];
  snprintf(payloadToSend, sizeof(payloadToSend),
          "{"
          "\"type\":2,"
          "\"anchorMacAddress\":\"%s\","
          "\"assetMacAddress\":\"%s\","
          "\"battery\":%d"
          "}",
          configManager.getMacAddress().c_str(),
          tag_id,
          battery_level);

  mqttSendModule(payloadToSend); // Publica no tópico de escrita
}

//---Tratamento de dados---
/* @brief Função que verifica se a string é vazia ou contém apenas espaços, \r, \n */
bool isEmptyOrSpaces(char *str){
  for (int i = 0; i < strlen(str); i++){
    char c = str[i];
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n'){
      return false; // Encontrou um caractere válido
    }
  }
  return true; // Só tem espaços ou está vazio
}

/* @brief Função para verificar se a string contém "AT" */
bool containsAT(char *str){
  for (int i = 0; i < strlen(str) - 1; i++){
    // Verifica se encontra 'A' seguido de 'T' (case insensitive)
    if ((str[i] == 'A' || str[i] == 'a') && (str[i + 1] == 'T' || str[i + 1] == 't')){
      return true;
    }
  }
  return false;
}

/* @brief Função para obter o MAC Address da âncora via comando AT */
void getAnchorMAC(){
  AnchorSerial.print("AT+UDFCFG=4\r");
  log_i("Aguardando MacAddress da âncora...");

  String response = "";
  unsigned long startTime = millis();

  // Aguarda receber os dados ou atingir timeout
  while (true){
    while (AnchorSerial.available()){
      char c = AnchorSerial.read();
      response += String(c);
      Serial.print(c);

      if (response.indexOf("UDFCFG") >= 0){

        // Remove aspas da resposta
        int start = response.indexOf('"');
        int end = response.indexOf('"', start + 1);

        if (start >= 0 && end > start){
          String mac = response.substring(start + 1, end);
          mac.toCharArray(anchor_id, sizeof(anchor_id));

          log_i("\n✅ MAC obtido: %s", anchor_id);

          return;
        }
      }
    }
    if (millis() - startTime > 5000)
    {
      log_e("\n❌ Timeout ao obter MAC da âncora.");
      String defaultMac = "000000000000";
      defaultMac.toCharArray(anchor_id, sizeof(anchor_id));
      return;
    }
    delay(10); // Pequena pausa para não sobrecarregar
  }
}

/* @brief Tratamento do nível de bateira. Ex: +UUDFP:20BA3607CA39,05FF64000000 -> O dispositivo 20BA3607CA39 está com 100% de bateria (0x64) 
 * @param input Conteúdo recebido da âncora
 */
void Publish_UUDFP(const char *input){
  String payload = String(input);
  payload = payload.substring(payload.indexOf("UUDFP:") + 6); // Remove "UUDFP:"
  String tag_id = payload.substring(0, payload.indexOf(",")); // Obtém o ID da tag
  String battery_hex = payload.substring(payload.indexOf(",") + 5, payload.indexOf(",") + 7); // Obtém o valor da bateria em hexadecimal
  int battery_level = strtol(battery_hex.c_str(), NULL, 16); // Converte de hexadecimal para decimal

  log_i("Tag: %s, Bateria: 0x%s (%d%%)\r\n", tag_id.c_str(), battery_hex.c_str(), battery_level);

  processNewBatteryReading(tag_id, battery_level);
}

/* @brief Informações de posição. Ex: +UUDF:D3EC5D5FB708,-73,-66,25,0,26,"20BA369B04AB","",4490117,12445 (TAG ID; RSSI; Azimuth; Elevation; XX ; Channel; Anchor ID; XX; timestamp; counter; Azimuth RAW; Elevation RAW) 
 * @param input Conteúdo recebido da âncora
 */
void Publish_UUDF(const char *input){
  char buffer[160];
  strncpy(buffer, input, sizeof(buffer));

  char *data = strstr(buffer, "UUDF:");
  if (!data) data = strstr(buffer, "+UUDF:");
  if (!data) return;
  data += 5;

  /*
  *  tokens[0] = Tag ID
  *  tokens[1] = RSSI
  *  tokens[2] = Azimuth
  *  tokens[3] = Elevation
  *  tokens[4] = Not Used
  *  tokens[5] = Channel
  *  tokens[6] = Anchor ID
  *  tokens[7] = Not Used
  *  tokens[8] = timestamp
  *  tokens[9] = counter
  *  tokens[10] = Azimuth RAW
  *  tokens[11] = Elevation RAW
  */
  char *tokens[12];
  
  char *token = strtok(data, ",");
  
  for (int i = 0; i < 12; i++) {
    tokens[i] = token;
    token = strtok(NULL, ",");
    if (token == NULL && i >= 9){ //Termina de preencher o array caso a âncora esteja com o parametro 121 desabilitado (Neste caso os indices 10 e 11 não são enviados)
      tokens[10] = tokens[2]; // Copia o valor de azimuth para azimuth_raw
      tokens[11] = tokens[3]; // Copia o valor de elevation para elevation_raw
      break;
    }
  }

  //Envia para o algoritmo de processamento de dados (filtro, média, etc) e posterior envio via MQTT
  if (useRawAngles) {
    log_i("Enviando para processamento: Tag: %s, Azimuth_RAW: %s, Elevation_RAW: %s, RSSI: %s\r\n", tokens[0], tokens[10], tokens[11], tokens[1]);
    processNewLocationReading(tokens[0], atof(tokens[10]), atof(tokens[11]), atof(tokens[1]));
  } else {
    log_i("Enviando para processamento: Tag: %s, Azimuth: %s, Elevation: %s, RSSI: %s\r\n", tokens[0], tokens[2], tokens[3], tokens[1]);
    processNewLocationReading(tokens[0], atof(tokens[2]), atof(tokens[3]), atof(tokens[1]));
  }

  //Altera status do led indicador:
  digitalWrite(GPIO_LED_PIN, !digitalRead(GPIO_LED_PIN));
}

/* ========= SETUP ========= */
void setup(){
  pinMode(GPIO_LED_PIN, OUTPUT);
  pinMode(BOT_ERASE, INPUT);

  Serial.begin(115200);
  Serial.setDebugOutput(true);

  AnchorSerial.begin(115200, SERIAL_8N1, RXD2, TXD2);

  #ifdef USING_NATIVE_USBC
    uart_set_pin(UART_NUM_2, TXD2, RXD2, RTS2, CTS2);
    uart_set_hw_flow_ctrl(UART_NUM_2, UART_HW_FLOWCTRL_CTS_RTS, 122);
  #endif

  log_d("UART2 configurada: 115200 8N1 com RTS/CTS");

  delay(2000); // Pequena pausa para garantir que a âncora esteja inicializada

  if (digitalRead(BOT_ERASE) == LOW) {
    log_w("Botão de reset pressionado. Apagando configurações de rede e reiniciando...");
    configureUbloxBoard();
    configManager.clearConfig();
    delay(1000);
    ESP.restart();
  }

  //Garante que todos os leds estejam apagados no início
  AnchorSerial.write("\rAT+ULED=1,0\r");
  AnchorSerial.write("AT+ULED=2,0\r");
  AnchorSerial.write("AT+ULED=3,0\r");
  delay(1000);

  getAnchorMAC();
  anchorGreenLed(1); //Acende o LED verde para indicar que a âncora está ligada e inicializada
  anchorRedLed(1); //Acende o LED vermelho até que a âncora se conecte ao MQTT
  AnchorSerial.write("AT+ULED=3,0\r"); //Apaga o led azul até que a âncora se conecte ao WiFi
  
  
  // ---Registro de callbacks do gerenciador de rede---
  // configManager.setOnMQTTConfigChangedCallback(MqttConfigChanged); // Caso seja necessário tratar a mudança de configuração do Broker MQTT em tempo de execução, registre este callback.
  configManager.setOnNetworkChangeCallback(networkChanged); // Callback unificado para tratar mudanças de rede (Wi-Fi ou Ethernet)

  // Inicia o gerenciador de rede:
  configManager.begin(); 
  configManager.FwVersion = FIRMWARE_VERSION;

  // Cria uma Task para rodar o loop do NetworkConfigManager em background, garantindo que o servidor web e a detecção de mudanças de rede funcionem corretamente.
  xTaskCreatePinnedToCore([](void *param) {
    while (true) {
        configManager.loop(); // Mantém o servidor web ativo e monitora o status do Wi-Fi.
        vTaskDelay(1 / portTICK_PERIOD_MS); // Pequena pausa para reset do watchdog
    }
  }, "ConfigManagLoop", 4096, NULL, 1, NULL, 0);

  /* MQTT */
  // Nesse momento o dispositivo está sem conexão de rede pois as funções Ethernet e Wifi não são bloqueantes
  // A conexão será realizada no loop() quando a rede for estabelecida
  
  /* Algoritmo de filtro e envio dos dados processados */
  setBatteryProcessorCallback(sendBatteryToBroker); // Função de callback dos dados de bateria
  setLocationProcessorCallback(sendLocationToBroker); // Função de callback dos dados de localização
  setupLocationProcessor(); // Inicializa a memória e a Task de background
}

/* ========= LOOP ========= */
void loop(){
  if (connectedToNetwork){
    if (!isMqttRunning) { // Caso o MQTT ainda não tenha sido iniciado
      log_w("Conexão de rede estabelecida. Iniciando conexão MQTT...");
      
      // Configura o MQTT com a função de callback para receber comandos no tópico especificado
      setupMQTTManager(anchorMqttCallback, configManager.getBrokerTopicRead(), configManager.getBrokerAddress(), configManager.getBrokerUser(), 
                       configManager.getBrokerPass(), configManager.getBrokerPort(), configManager.getUseTLS(), configManager.getCertType(), 
                       configManager.getCACert(), configManager.getClientCert(), configManager.getClientKey());

      isMqttRunning = true; // Salva que o MQTT foi iniciado (independente se conectou ou não)
    }   

    loopMQTTManager(); //Processa a conexão MQTT em background e realiza reconexão se necessário

    if(!isMQTTConnected()){
      anchorRedLed(true); // Acende o LED vermelho para indicar que a âncora perdeu a conexão com o MQTT
      configManager.changeMqttState(false); // Salva que o MQTT não está conectado
    } else {
      anchorRedLed(false); // Apaga o LED vermelho para indicar que a âncora está conectada ao MQTT
      configManager.changeMqttState(true); // Salva que o MQTT está conectado
    
      // Lê dados do Anchor
      while (AnchorSerial.available() > 0){
        char c = AnchorSerial.read();
        Serial.write(c); // Debug

        if (uartIndex >= UART_BUFFER_SIZE - 1) uartIndex = 0;

        if (c == '\n'){
          uartBuffer[uartIndex] = '\0';

          if (strstr(uartBuffer, "UUDF:")) Publish_UUDF(uartBuffer); // Dados de localização
          else if (strstr(uartBuffer, "UUDFP") != NULL) Publish_UUDFP(uartBuffer); // Dados de nível de bateria
          else if ((strstr(uartBuffer, "OK") == NULL) && !containsAT(uartBuffer) && !isEmptyOrSpaces(uartBuffer)) { // Retorno de comandos (não é necessário tranmitir para o broker)
            char topic[64];
            snprintf(topic, sizeof(topic), configManager.getBrokerTopicWrite().c_str());
            // mqtt.publish(topic, uartBuffer);
            String content = "{\"type\":12,\"content\":\"" + String(uartBuffer) + "\"}";
            mqttSendModule(content.c_str());
          }

          uartIndex = 0;
        } else if (c != '\r') {
          uartBuffer[uartIndex++] = c;
        }
      }
    }
  } else {
    anchorRedLed(true); // Acende o LED vermelho para indicar que a âncora perdeu a conexão com o Wi-Fi

    // Lê dados da âncora e envia para UART0
    while (AnchorSerial.available() > 0){
      char c = AnchorSerial.read();
      Serial.write(c);
    }
  }

  // Encaminha dados recebidos da UART0 para a âncora
  while (Serial.available()){
    char c = Serial.read();
    if (c != 10) AnchorSerial.write(c); //A âncora não espera receber '\n' ao final dos comandos
  }
}