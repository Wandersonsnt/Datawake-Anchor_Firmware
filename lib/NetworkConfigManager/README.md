# NetworkConfigManager (ESP32-S3)

O **NetworkConfigManager** é uma biblioteca robusta em C++ desenvolvida para o framework Arduino no ambiente PlatformIO. Ela fornece uma solução completa para gerenciamento de rede Wi-Fi e configurações do Broker MQTT em dispositivos da família ESP32 (focada no ESP32-S3).

#### Criado por: Wanderson Santos Souza - SENAI DT

## Principais Funcionalidades

* **Ponto de Acesso Inteligente (AP/STA):** Cria automaticamente um Access Point de configuração caso o ESP32 não consiga se conectar à rede Wi-Fi salva, e oculta o AP assim que a conexão é restabelecida.
* **Credenciais Únicas por Hardware:** Gera um SSID e uma senha para o Access Point baseados no endereço MAC do próprio dispositivo, garantindo que dezenas de placas no mesmo ambiente não entrem em conflito.
  * Exemplo de SSID: `WK-ANCHOR-AABBCC` (onde AABBCC são os últimos 3 bytes do MAC).
  * Exemplo de Senha: `AdminABC` (onde ABC é o último nibble de cada byte).
* **Interface Web Assíncrona e Nativa (SPIFFS):** Utiliza o sistema de arquivos flash do ESP32 para servir arquivos HTML genéricos. A página principal busca redes dinamicamente e salva dados sem recarregar a tela (via Fetch API).
* **Varredura de Redes (Scan):** Mapeia ativamente os SSIDs ao redor (indicando força do sinal RSSI e presença de cadeado de segurança) e envia ao front-end em formato JSON.
* **Hot-Swap MQTT via Callbacks:** Salva as configurações persistentes via NVS (`Preferences`). Possui ponteiros de função que informam ao programa principal (`main.cpp`) sempre que a rede cai, volta ou quando os dados do MQTT são atualizados pelo usuário na página Web, permitindo reconexão sem a necessidade de reiniciar o hardware.

---

## Estrutura de Diretórios Recomendada no PlatformIO

Para que a biblioteca funcione corretamente e sirva a interface web, certifique-se de manter os arquivos HTML, CSS e imagens dentro da pasta `data/` na raiz do seu projeto.

```text
meu_projeto_iot/
├── data/
│   ├── index.html        # Interface de configuração (com as variáveis %TAG%)
│   ├── style.css         # Estilos opcionais
│   └── logo.png          # Imagens servidas via SPIFFS
├── lib/
│   └── NetworkConfigManager/
│       ├── NetworkConfigManager.h
│       ├── NetworkConfigManager.cpp
│       └── README.md
├── src/
│   └── main.cpp          # Lógica principal da sua aplicação
└── platformio.ini
```

**Atenção:** É obrigatório realizar o envio do sistema de arquivos para o ESP32. No VS Code, utilize a aba do PlatformIO e clique em `Build Filesystem Image` seguido de `Upload Filesystem Image`.

## Como Utilizar
### 1. Inicialização e Mapeamento de Callbacks (main.cpp)
A biblioteca encapsula toda a lógica complexa de rotas, HTML e Wi-Fi. O seu main.cpp precisa apenas reagir aos eventos:

```C++
#include <Arduino.h>
#include "NetworkConfigManager.h"

NetworkConfigManager networkManager;

// --- Callbacks de Eventos da Rede ---

void aoConectarWiFi() {
    Serial.println("[MAIN] Wi-Fi conectado! Iniciando serviços dependentes de rede...");
    // Conectar ao Broker MQTT aqui...
}

void aoDesconectarWiFi() {
    Serial.println("[MAIN] Wi-Fi desconectado! Parando envios...");
    // Sinalizar falha (LED vermelho, pausa em leituras pesadas, etc.)
}

void aoMudarConfigMQTT() {
    Serial.println("[MAIN] Configurações MQTT alteradas via Web! Aplicando a quente...");
    
    String novoBroker = networkManager.getBrokerAddress();
    int novaPorta = networkManager.getBrokerPort();
    String writeTopic = networkManager.getBrokerTopicWrite();
    
    Serial.printf("Novo Broker: %s:%d | Tópico TX: %s\n", novoBroker.c_str(), novaPorta, writeTopic.c_str());
    
    // Desconectar do client MQTT atual e conectar com os novos dados...
}

// --- Setup e Loop ---

void setup() {
    Serial.begin(115200);

    // Registra as funções de callback ANTES do begin()
    networkManager.setOnConnectCallback(aoConectarWiFi);
    networkManager.setOnDisconnectCallback(aoDesconectarWiFi);
    networkManager.setOnMQTTConfigChangedCallback(aoMudarConfigMQTT);
    
    // Inicia o SPIFFS, tenta conectar ao Wi-Fi salvo e sobe o WebServer
    networkManager.begin();
}

void loop() {
    // Obrigatório: processa os clientes Web e avalia o status do Wi-Fi dinamicamente
    networkManager.loop();

    // Restante da lógica do seu dispositivo...
}
```
## API do Servidor Web Integrado
Os seguintes endpoints são registrados na inicialização do WebServer interno para interagir com o Front-End:

* `GET /`: Serve a página web principal (lendo `/index.html` do SPIFFS e substituindo os marcadores com os dados atuais).

* `GET /scan_wifi`: Executa o mapeamento de espectro e devolve uma string JSON contendo nome, segurança e força (RSSI) de cada rede próxima.

* `POST /save_wifi`: Recebe as chaves `sta_ssid` e `sta_pass`. Salva na memória persistente e reinicia o ESP32.

* `POST /save_mqtt`: Recebe credenciais e tópicos do broker (`broker_path`, `broker_port`, `broker_pass`, `broker_topic_write`, `broker_topic_read`). Atualiza as variáveis em RAM e dispara o callback Hot-Swap, sem necessidade de reinício.

* `GET /restart`: Força a reinicialização segura do equipamento.

## Dependências
A biblioteca utiliza exclusivamente componentes nativos do Core do Arduino para ESP32, não sendo necessária a instalação de bibliotecas externas pelo arquivo `platformio.ini`:

* `WiFi.h`

* `WebServer.h`

* `Preferences.h`

* `SPIFFS.h`