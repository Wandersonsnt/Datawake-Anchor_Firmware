#include "NetworkConfigManager.h"

SPIClass spiEthernet(HSPI); // HSPI = SPI3_HOST

NetworkConfigManager::NetworkConfigManager() : server(80), lastWiFiState(false) {}

/* @brief Trata requisições para URLs que não existem, redirecionando para o Captive Portal */
void NetworkConfigManager::handleNotFound() {
    // Retorna um código HTTP 302 (Redirecionamento) apontando para o IP do AP do ESP32
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    server.send(302, "text/plain", "");
}

/* @brief Trata requisições para URLs de arquivos confidenciais (certificados) */
void NetworkConfigManager::handleCertRequest() {
    // Retorna um código HTTP 403 (Proibido) para impedir o acesso a arquivos confidenciais
    server.send(403, "text/plain", "Acesso negado");
    
    
}

/* @brief Limpa as configurações de rede e reinicia o dispositivo*/
void NetworkConfigManager::clearConfig() {
    log_i("Limpando configurações do dispositivo...");
    preferences.begin(DEFAULT_NVS_NAMESPACE, false); // Abre a NVS para escrita
    preferences.clear(); // Limpa todas as configurações
    preferences.end();
    log_w("Configurações limpas. Reiniciando...");
    delay(1000);
    ESP.restart(); // Reinicia o dispositivo
}

/* @brief Inicializa o gerenciador de configuração de rede 
 * @param nvsNamespace Namespace para armazenar as configurações na NVS
 */
void NetworkConfigManager::begin() {
    if (!SPIFFS.begin(true)) {
        log_e("Erro ao montar o SPIFFS!");
    }
    loadConfig();

    // 1. REGISTRA A CAPTURA DE EVENTOS (Cabo plugado, Wi-Fi conectado, etc)
    WiFi.onEvent([this](arduino_event_id_t event, arduino_event_info_t info) {
        this->handleNetworkEvent(event, info);
    });

    // 2. INICIALIZA A ETHERNET (Se habilitada no painel)
    if (useEthernet) {
        // Inicia o barramento SPI explicitamente com os pinos da placa
        spiEthernet.begin(W5500_SCK, W5500_MISO, W5500_MOSI, -1);

        // Se DHCP estiver desligado e houver um IP configurado, aplica o IP Estático
        if (!ethDhcp && ethIp.length() > 0) {
            IPAddress ip, gateway, subnet;
            ip.fromString(ethIp);
            subnet.fromString(ethMask);
            gateway.fromString(ethGw);
            ETH.config(ip, gateway, subnet);
            log_d("Ethernet configurada com IP Estático: %s", ethIp.c_str());
        } else {
            log_d("Ethernet configurada para DHCP.");
        }
        
        if (!ETH.begin(ETH_PHY_W5500, 1, W5500_CS, W5500_INT, W5500_RST, spiEthernet)) {
            log_e("Falha ao inicializar módulo Ethernet!");
        }
    }

    if (useWiFi && staSSID.length() > 0) {
        WiFi.mode(WIFI_AP_STA); // Modo AP + Estação simultaneamente

        // Se DHCP estiver desligado e houver um IP configurado, aplica o IP Estático
        if (!wifiDhcp && wifiIp.length() > 0) {
            IPAddress ip, gateway, subnet;
            ip.fromString(wifiIp);
            subnet.fromString(wifiMask);
            gateway.fromString(wifiGw);
            WiFi.config(ip, gateway, subnet);
            log_d("Wi-Fi configurado com IP Estático: %s", wifiIp.c_str());
        } else {
            log_d("Wi-Fi configurado para DHCP.");
        }

        log_i("Wi-Fi habilitado. Iniciando conexao em background com: %s - %s\n", staSSID.c_str(), staPassword.c_str());
        WiFi.begin(staSSID.c_str(), staPassword.c_str());
        log_d("Iniciando AP visível.");
        setupAP(false); // Inicializa AP com SSID visível
        lastWiFiState = false; //Como a função de conexão com o Wi-Fi é assíncrona, o estado inicial é considerado desconectado até que a função de callback seja chamada.
    } else {
        log_d("Wi-Fi desabilitado. Iniciando apenas AP.");
        WiFi.mode(WIFI_AP); // Modo AP apenas
        setupAP(false); // Inicializa AP com SSID visível
        lastWiFiState = false;
    }

    log_v("Iniciando servidor DNS.");
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP()); // Inicia o DNS para o Captive Portal

    // Rotas do Servidor Web
    server.on("/", HTTP_GET, std::bind(&NetworkConfigManager::handleRoot, this));
    server.on("/scan_wifi", HTTP_GET, std::bind(&NetworkConfigManager::handleScanWiFi, this));
    server.on("/save_network", HTTP_POST, std::bind(&NetworkConfigManager::handleSaveNetwork, this));
    server.on("/restart", HTTP_GET, std::bind(&NetworkConfigManager::handleRestart, this));
    server.on("/factory_reset", HTTP_GET, std::bind(&NetworkConfigManager::handleFactoryReset, this));
    server.on("/ca.pem", HTTP_GET, std::bind(&NetworkConfigManager::handleCertRequest, this));
    server.on("/client.pem", HTTP_GET, std::bind(&NetworkConfigManager::handleCertRequest, this));
    server.on("/key.pem", HTTP_GET, std::bind(&NetworkConfigManager::handleCertRequest, this));
    server.on("/save_mqtt", HTTP_POST, 
        std::bind(&NetworkConfigManager::handleSaveMQTT, this),  // Processa os textos do formulário
        std::bind(&NetworkConfigManager::handleMqttUpload, this) // Processa os arquivos enviados via upload
    );
    server.on("/update", HTTP_POST, 
        std::bind(&NetworkConfigManager::handleOTAUpdate, this), // Processa o fim do upload do firmware
        std::bind(&NetworkConfigManager::handleOTAUpload, this)  // Processa os pacotes do upload do firmware
    );
    
    // Se o dispositivo tentar acessar qualquer URL que não existe, joga para o Captive Portal
    server.onNotFound(std::bind(&NetworkConfigManager::handleNotFound, this));

    // Rota para arquivos (CSS, JS e Imagens se existirem)
    server.serveStatic("/", SPIFFS, "/");

    server.begin();
    
    log_i("Servidor de configuração iniciado. SSID: %s, Senha: %s, IP: %s\n", apSSID.c_str(), apPassword.c_str(), WiFi.softAPIP().toString().c_str());
}

/* @brief Mantém o servidor web ativo e monitora o status do Wi-Fi */
void NetworkConfigManager::loop() {
    server.handleClient();

    dnsServer.processNextRequest(); // Processa os redirecionamentos de DNS

    // Verifica se o dispositivo está  online com QUALQUER interface (Cabo ou Wi-Fi)
    bool isOnline = (activeInterface != NET_NONE);
    
    if (isOnline != lastWiFiState) { 
        if (isOnline) {
            log_i("Conexão de rede estabelecida! Ocultando AP de configuracao...");
            setupAP(true);
        } else {
            log_i("Sem conexões ativas! Tornando AP de configuracao visivel...");
            setupAP(false);
        }
        lastWiFiState = isOnline;
    }
}

/* @brief Configura o relógio interno do ESP32 via NTP */
void NetworkConfigManager::setupTime(){
  log_v("Iniciando sincronização do relógio via NTP...\n");
  configTime(-3 * 3600, 0, "a.st1.ntp.br", "8.8.8.8");
}

/* @brief Retorna o timestamp atual */
time_t NetworkConfigManager::getCurrentTimestamp() {
    return time(nullptr);
}

/* @brief Configura o ponto de acesso (AP) do ESP32 
   @param hidden Indica se o AP deve ser oculto (True) ou visível (False)
*/
void NetworkConfigManager::setupAP(bool hidden) {
    char ssidBuf[32];
    char passBuf[32];
    snprintf(ssidBuf, sizeof(ssidBuf), "WK-ANCHOR-%02X%02X%02X", macAddress[3], macAddress[4], macAddress[5]);
    snprintf(passBuf, sizeof(passBuf), "Admin%X%X%X", (macAddress[3] & 0x0F), (macAddress[4] & 0x0F), (macAddress[5] & 0x0F));
    apSSID = String(ssidBuf);
    apPassword = String(passBuf);

    WiFi.softAP(apSSID.c_str(), apPassword.c_str(), 1, hidden, 4);
}

/* @brief Carrega as configurações salvas no armazenamento não volátil */
void NetworkConfigManager::loadConfig() {
    //MacAddress
    esp_read_mac(macAddress, ESP_MAC_WIFI_STA);
    char macStr[12] = {0};
    sprintf(macStr, "%02X%02X%02X%02X%02X%02X", macAddress[0], macAddress[1], macAddress[2], macAddress[3], macAddress[4], macAddress[5]);
    macAddressStr = String(macStr);
    
    preferences.begin(DEFAULT_NVS_NAMESPACE, true); // Somente leitura

    // Rede Ethernet
    useEthernet = preferences.getBool("use_eth", DEFAULT_USE_ETHERNET);
    ethDhcp = preferences.getBool("e_dhcp", DEFAULT_ETH_DHCP); // Default é DHCP ligado
    ethIp = preferences.getString("e_ip", DEFAULT_ETH_IP);
    ethMask = preferences.getString("e_mask", DEFAULT_ETH_MASK);
    ethGw = preferences.getString("e_gw", DEFAULT_ETH_GW);

    // Rede Wi-Fi
    useWiFi = preferences.getBool("use_wifi", DEFAULT_USE_WIFI);
    staSSID = preferences.getString("ssid", DEFAULT_WIFI_SSID);
    staPassword = preferences.getString("password", DEFAULT_WIFI_PASSWORD);
    wifiDhcp = preferences.getBool("w_dhcp", DEFAULT_WIFI_DHCP); // Default é DHCP ligado
    wifiIp = preferences.getString("w_ip", DEFAULT_WIFI_IP);
    wifiMask = preferences.getString("w_mask", DEFAULT_WIFI_MASK);
    wifiGw = preferences.getString("w_gw", DEFAULT_WIFI_GW);

    // Broker MQTT
    brokerAddress = preferences.getString("b_path", DEFAULT_BROKER_PATH);
    brokerUser = preferences.getString("b_user", DEFAULT_BROKER_USER);
    brokerPass = preferences.getString("b_pass", DEFAULT_BROKER_PASSWORD);
    brokerPort = preferences.getInt("b_port", DEFAULT_BROKER_PORT);
    brokerTopicWrite = preferences.getString("b_tw", DEFAULT_BROKER_TOPIC_WRITE);
    brokerTopicRead = preferences.getString("b_tr", DEFAULT_BROKER_TOPIC_READ);
    brokerUseTLS = preferences.getBool("b_tls", DEFAULT_BROKER_USE_TLS);
    brokerCertFromFactory = preferences.getBool("b_cert_factory", DEFAULT_BROKER_CERT_FROM_FACTORY);
    preferences.end();
}

/* @brief Trata a requisição da página principal */
void NetworkConfigManager::handleRoot() {
    if (!SPIFFS.exists("/index.html")) {
        server.send(404, "text/plain", "Erro: index.html não encontrado. Necessário gravação do SPIFFS no dispositivo.");
        return;
    }

    File file = SPIFFS.open("/index.html", "r");
    String html = file.readString();
    file.close();

    // 1. Preparação das Tags de Status
    String networkConnection = activeInterface==0 ? "<span style='color: #dc3545;'> Desconectado" : ((activeInterface==1 ? "<span style='color: #28a745;'> WiFi Conectado (":"<span style='color: #28a745;'> Ethernet Conectado (") + getLocalIP() + ")</span>");
    String wifiStatusStr = isWiFiConnected() ? "<span style='color: #28a745;'>Conectado (" + staSSID + ")</span>" : "<span style='color: #dc3545;'>Desconectado</span>";
    String mqttStatusStr = lastMQTTState ? "<span style='color: #28a745;'>Conectado</span>" : "<span style='color: #dc3545;'>Desconectado</span>";

    // 2. Substituição das tags no HTML
    html.replace("%NETWORK_STATE%", networkConnection); 
    html.replace("%ETH_CHECKED%", useEthernet ? "checked" : "");
    html.replace("%WIFI_CHECKED%", useWiFi ? "checked" : "");

    // Ethernet Tags
    html.replace("%ETH_DHCP_CHECKED%", ethDhcp ? "checked" : "");
    html.replace("%ETH_IP%", ethIp);
    html.replace("%ETH_MASK%", ethMask);
    html.replace("%ETH_GW%", ethGw);

    // Wi-Fi Tags
    html.replace("%CURRENT_WIFI_SSID%", staSSID);
    html.replace("%WIFI_DHCP_CHECKED%", wifiDhcp ? "checked" : "");
    html.replace("%WIFI_IP%", wifiIp);
    html.replace("%WIFI_MASK%", wifiMask);
    html.replace("%WIFI_GW%", wifiGw);

    // MQTT Tags
    html.replace("%MQTT_STATUS%", mqttStatusStr);
    html.replace("%CURRENT_BROKER_PATH%", brokerAddress);
    html.replace("%CURRENT_BROKER_USER%", brokerUser);
    html.replace("%CURRENT_BROKER_PORT%", String(brokerPort));
    html.replace("%CURRENT_BROKER_TOPIC_WRITE%", brokerTopicWrite);
    html.replace("%CURRENT_BROKER_TOPIC_READ%", brokerTopicRead);
    html.replace("%MAC_ADDRESS%", macAddressStr);
    html.replace("%TLS_CHECKED%",NetworkConfigManager::getUseTLS() ? "checked" : "");
    html.replace("%CERT_FACTORY_SELECTED%", NetworkConfigManager::getCertType() == true ? "selected" : "");
    html.replace("%CERT_CUSTOM_SELECTED%", NetworkConfigManager::getCertType() == false ? "selected" : "");
    // Nota: Por segurança, as senhas não são enviadas para o HTML (deixando os placeholders vazios).

    // OTA
    html.replace("%FW_VERSION%", FwVersion);

    server.send(200, "text/html", html);
}

/* @brief Trata a requisição de varredura da rede Wi-Fi */
void NetworkConfigManager::handleScanWiFi() {

    log_i("Iniciando scan de redes Wi-Fi...");

    if (WiFi.status() != WL_CONNECTED) { // O scan não funciona corretamente se o wifi estiver habilitado e desconectado, então é necessário desconectar o STA temporariamente
        WiFi.disconnect();
        delay(100);
        log_v("Desativado reconexão na rede Wi-Fi atual...");
    }

    int n = WiFi.scanNetworks();
    log_i("Scan Wi-Fi completo. Redes encontradas: %d.\n", n);
    
    // Construção do JSON:
    String json = "[";
    for (int i = 0; i < n; ++i) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
        json += "\"secure\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? 0 : 1);
        json += "}";

        log_i("   Rede %d: SSID: %s, RSSI: %d, Segura: %s\n", i+1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "Não" : "Sim"));
    }
    json += "]";
    
    server.send(200, "application/json", json);
    
    WiFi.scanDelete(); // Limpa o scan da memória

    if (WiFi.status() != WL_CONNECTED && staSSID.length() > 0){ // Habilita reconexão à rede Wi-Fi anterior
        WiFi.reconnect(); 
    }
}

/* @brief Trata a requisição de salvamento das configurações de rede */
void NetworkConfigManager::handleSaveNetwork() {
    preferences.begin(DEFAULT_NVS_NAMESPACE, false);

    //Ethernet
    bool useEth = server.hasArg("use_eth"); //Se o checkbox não vem no request, é porque está desmarcado
    preferences.putBool("use_eth", useEth);
    if (useEth){
        bool ethDhcp = server.hasArg("eth_dhcp");
        preferences.putBool("e_dhcp", ethDhcp);
        if (!ethDhcp) { // Se o usuário desmarcou o DHCP, salva os IPs estáticos
            if (server.hasArg("eth_ip")) preferences.putString("e_ip", server.arg("eth_ip"));
            if (server.hasArg("eth_mask")) preferences.putString("e_mask", server.arg("eth_mask"));
            if (server.hasArg("eth_gw")) preferences.putString("e_gw", server.arg("eth_gw"));
        }
    }

    //WiFi
    bool useWifi = server.hasArg("use_wifi");
    preferences.putBool("use_wifi", useWifi);
    if (useWifi){
        bool wifiDhcp = server.hasArg("wifi_dhcp");
        preferences.putBool("w_dhcp", wifiDhcp);
        if (!wifiDhcp) { // Se o usuário desmarcou o DHCP, salva os IPs estáticos
            if (server.hasArg("wifi_ip")) preferences.putString("w_ip", server.arg("wifi_ip"));
            if (server.hasArg("wifi_mask")) preferences.putString("w_mask", server.arg("wifi_mask"));
            if (server.hasArg("wifi_gw")) preferences.putString("w_gw", server.arg("wifi_gw"));
        }

        if (server.hasArg("sta_ssid")) { // Apenas atualiza se o usuário digitou um novo SSID
            preferences.putString("ssid", server.arg("sta_ssid"));
            
            // Só salva a senha se o usuário digitou uma nova
            if (server.hasArg("sta_pass") && server.arg("sta_pass").length() > 0) {
                preferences.putString("password", server.arg("sta_pass"));
            }
        }
    }

    preferences.end();
    log_w("Configurações de rede atualizadas. Serão aplicadas após a próxima reinicialização.");
    server.send(200, "text/plain", "OK");
}

/* @brief Trata a requisição de salvamento das configurações do Broker MQTT */
void NetworkConfigManager::handleSaveMQTT() {
    preferences.begin(DEFAULT_NVS_NAMESPACE, false);
    
    preferences.putString("b_path", server.arg("broker_path"));
    preferences.putInt("b_port", server.arg("broker_port").toInt());
    preferences.putString("b_user", server.arg("broker_user"));
    preferences.putString("b_tw", server.arg("broker_topic_write"));
    preferences.putString("b_tr", server.arg("broker_topic_read"));

    if (server.hasArg("broker_pass") && server.arg("broker_pass").length() > 0) {
        preferences.putString("b_pass", server.arg("broker_pass"));
    }
    
    if (server.hasArg("use_tls")) {
        preferences.putBool("b_tls", true);
    } else {
        preferences.putBool("b_tls", false);
    }

    if (server.hasArg("cert_type")) {
        preferences.putBool("b_cert_factory", server.arg("cert_type") == "factory" ? true : false);
    }
    
    preferences.end();
    log_w("Configurações do MQTT atualizadas. Serão aplicadas após a próxima reinicialização.");
    server.send(200, "text/plain", "OK");
    
    // Dispara o callback para mudar de broker em tempo de execução (Ou aguarda reinicialização caso não tiver configurado o callback)
    if (onMQTTConfigChangedCallback != nullptr) {
        loadConfig(); // Atualiza as variáveis internas com os novos valores
        onMQTTConfigChangedCallback();
    }
}

/* @brief Trata o upload dos arquivos de certificados MQTT */
void NetworkConfigManager::handleMqttUpload() {
    HTTPUpload& upload = server.upload();

    // O recebimento do arquivo é feito em fragmentos, então o upload.status indica qual parte do pacote está sendo recebido.
    if (upload.status == UPLOAD_FILE_START) { //Primeiro pacote do arquivo -> Preparação do recebimento
        // Se o usuário não enviou nenhum arquivo no campo, o campo filename é vazio. Aborta para não criar arquivos em branco
        if (upload.filename.length() == 0) return; // Garante que não sobrescreva um certificado já salvo com um arquivo de 0 bytes.

        String inputName = upload.name; // Recebe o atributo "name" do input HTML
        String savePath = ""; // Caminho onde o arquivo será salvo no SPIFFS

        // Padronização dos nomes para não depender do nome do arquivo original criado pelo usuário
        if (inputName == "ca_cert") savePath = "/ca.pem";
        else if (inputName == "client_cert") savePath = "/client.pem";
        else if (inputName == "client_key") savePath = "/key.pem";
        else savePath = "/" + upload.filename;
        
        log_i("Recebendo certificado %s -> Salvando em: %s\n", upload.filename.c_str(), savePath.c_str());

        // Se já existir um arquivo aberto na memória de um pacote anterior que falhou, será fechado para não corromper o próximo arquivo que será salvo.
        if (uploadFile) {
            uploadFile.close();
        }

        // Abre o arquivo para escrita (sobrescreve se já existir)
        uploadFile = SPIFFS.open(savePath, FILE_WRITE);

        if (!uploadFile) {
            log_e("ERRO: Falha ao abrir %s para gravação no SPIFFS!", savePath.c_str());
        }
        
    } else if (upload.status == UPLOAD_FILE_WRITE) { // Pacotes intermediários do arquivo -> Adiciona no arquivo já criado
        // Tenta escrever apenas se o arquivo foi criado com sucesso no passo anterior
        if (uploadFile) {
            uploadFile.write(upload.buf, upload.currentSize);
        }
    } else if (upload.status == UPLOAD_FILE_END) { // Último pacote do arquivo -> Fecha o arquivo e finaliza o upload
        if (uploadFile) {
            uploadFile.close();
            log_i("Upload concluído: %s (%u bytes)\n", upload.name.c_str(), upload.totalSize);
        }
    }
}

/* @brief Trata a requisição de reinício do dispositivo */
void NetworkConfigManager::handleRestart() {
    server.send(200, "text/plain", "OK");
    log_w("Reiniciando o dispositivo a partir de uma requisição da página WEB...");
    server.client().stop(); // Encerra a conexão TCP com o navegador
    delay(500);
    WiFi.disconnect(true);
    delay(500);
    ESP.restart();
}

/* @brief Trata a requisição de redefinição de fábrica */
void NetworkConfigManager::handleFactoryReset() {
    server.send(200, "text/plain", "OK");
    log_w("Reiniciando o dispositivo e limpando as configurações de rede a partir de uma requisição da página WEB...");
    WiFi.disconnect(true);
    delay(1500);
    clearConfig();
}

/* @brief Callback que trata os eventos de rede
 * @param event ID do evento de rede
 * @param info descrição do evento
 */
void NetworkConfigManager::handleNetworkEvent(arduino_event_id_t event, arduino_event_info_t info) {
    log_w("Evento: %s (ID: %d)", Network.eventName(event), event);
    switch (event) {
        case ARDUINO_EVENT_ETH_START:
            ETH.setHostname("ESP32_SC_W5500");
            break;

        case ARDUINO_EVENT_ETH_GOT_IP:
            log_i("Ethernet Conectada! ETH MAC: %s. IP: %s. %s_DUPLEX, %dMbps.", ETH.macAddress().c_str(), ETH.localIP().toString().c_str(), ETH.fullDuplex() ? "FULL" : "HALF", ETH.linkSpeed());
            ethConnected = true;

            // PRIORIDADE: Se o cabo conectou, desabilitar o cliente Wi-Fi (O LwIP será forçado a rotear o tráfego pelo hardware Ethernet).
            if (useWiFi && WiFi.status() == WL_CONNECTED) {
                log_d("Prioridade Cabo: Desconectando Wi-Fi (Standby)...");
                WiFi.disconnect();
            }

            if (activeInterface != NET_ETH) {
                activeInterface = NET_ETH;
                if (onNetworkChangeCallback) onNetworkChangeCallback(true, "ETH");
            }

            setupTime();

            break;
            
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            log_i("Cabo Ethernet Desconectado!");
            ethConnected = false;

            // Fallback imediato para Wi-Fi se estiver conectado
            if (useWiFi && staSSID.length() > 0) {
                log_d("Fallback: Acionando Wi-Fi...");
                WiFi.begin(staSSID.c_str(), staPassword.c_str());
            }

            // Informa que está temporariamente sem rede (aguardando o Wi-Fi conectar se estiver habilitado):
            if (activeInterface){
                activeInterface = NET_NONE;
                if (onNetworkChangeCallback) onNetworkChangeCallback(false, "NONE");
            }
            break;

        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            log_w("Wi-Fi Conectado! IP: %s.", WiFi.localIP().toString().c_str());
            wifiConnected = true;

            // Só assume como interface ativa se a ETH não estiver conectada
            if (!ethConnected && activeInterface != NET_WIFI) {
                activeInterface = NET_WIFI;
                if (onNetworkChangeCallback) onNetworkChangeCallback(true, "WIFI");
            } else if (ethConnected) {
                // Prevenção de segurança técnica: se por acaso o Wi-Fi conectou no boot enquanto a Ethernet já tinha IP, derruba o Wi-Fi novamente.
                WiFi.disconnect();
            }

            setupTime();
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            wifiConnected = false;
            
            // Verifica se o wifi foi desativado pelo fato do Ethernet estar conectado:
            if (!ethConnected && activeInterface) {
                activeInterface = NET_NONE;
                if (onNetworkChangeCallback) onNetworkChangeCallback(false, "NONE");
            }
            break;
    }
}

/* @brief Verifica se o dispositivo está conectado à rede Wi-Fi
 * @return true se conectado, false se desconectado
 */
bool NetworkConfigManager::isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

/* @brief Altera o status da conexão MQTT na visualização da página web
 * @param connected true se conectado, false se desconectado
 */
void NetworkConfigManager::changeMqttState(bool connected){
    lastMQTTState = connected;
}

/* @brief Manipula a atualização OTA */
void NetworkConfigManager::handleOTAUpdate() {
    // A rota principal só envia a resposta de Sucesso/Falha depois que o upload termina
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    
    // Se a atualização foi bem sucedida, agenda o reinício
    if (!Update.hasError()) {
        delay(1000);
        ESP.restart();
    }
}

/* @brief Manipula o upload do arquivo de imagem da partição */
void NetworkConfigManager::handleOTAUpload() {
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        log_i("Iniciando Atualizacao OTA: %s\n", upload.filename.c_str());
        
        // Verifica se o usuário selecionou Firmware ou SPIFFS através da URL (?cmd=spiffs)
        int cmd = (server.arg("cmd") == "spiffs") ? U_SPIFFS : U_FLASH;

        // Inicia a gravação (UPDATE_SIZE_UNKNOWN gerencia tamanhos dinâmicos)
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) { 
            Update.printError(Serial);
        }
    } 
    else if (upload.status == UPLOAD_FILE_WRITE) {
        // Grava os blocos conforme chegam da rede
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } 
    else if (upload.status == UPLOAD_FILE_END) {
        // Finaliza o bloco e configura o ESP para dar boot na nova partição
        if (Update.end(true)) { 
            log_i("Recebimento do arquivo concluído com sucesso! Tamanho total: %u bytes\n", upload.totalSize);
        } else {
            Update.printError(Serial);
        }
    }
}

// Métodos para obter as configurações através do arquivo main.cpp:
String NetworkConfigManager::getBrokerAddress() { return brokerAddress; }
int NetworkConfigManager::getBrokerPort() { return brokerPort; }
String NetworkConfigManager::getBrokerUser() { return brokerUser; }
String NetworkConfigManager::getBrokerPass() { return brokerPass; }
String NetworkConfigManager::getBrokerTopicWrite() { return brokerTopicWrite + "/" + macAddressStr; }
String NetworkConfigManager::getBrokerTopicRead() { return brokerTopicRead + "/" + macAddressStr; }
bool NetworkConfigManager::getUseTLS() { return brokerUseTLS; }
bool NetworkConfigManager::getCertType() { return brokerCertFromFactory; }
String NetworkConfigManager::getCACert() {
    if (!SPIFFS.exists("/ca.pem")) return "";
    File f = SPIFFS.open("/ca.pem", FILE_READ);
    String cert = f.readString();
    f.close();
    return cert;
}
String NetworkConfigManager::getClientCert() {
    if (!SPIFFS.exists("/client.pem")) return "";
    File f = SPIFFS.open("/client.pem", FILE_READ);
    String cert = f.readString();
    f.close();
    return cert;
}
String NetworkConfigManager::getClientKey() {
    if (!SPIFFS.exists("/key.pem")) return "";
    File f = SPIFFS.open("/key.pem", FILE_READ);
    String cert = f.readString();
    f.close();
    return cert;
}
byte NetworkConfigManager::getCurrentConnection() {
    return activeInterface;
}
String NetworkConfigManager::getLocalIP() {
    if (activeInterface == NET_ETH) return  ETH.localIP().toString();
    if (activeInterface == NET_WIFI) return  WiFi.localIP().toString();
    return " ";
}
String NetworkConfigManager::getMacAddress() { return macAddressStr; }

//Callbacks:
void NetworkConfigManager::setOnMQTTConfigChangedCallback(MQTTCallback callback) { onMQTTConfigChangedCallback = callback; }
void NetworkConfigManager::setOnNetworkChangeCallback(NetworkChangeCallback callback) { onNetworkChangeCallback = callback; }