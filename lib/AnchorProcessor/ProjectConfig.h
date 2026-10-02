#define USING_NATIVE_USBC // Precisa ser definido para utilizar a comunição serial via USB nativo do ESP32-S3 (Comentar quando utilizar kit de desenvolvimento com conversor USB-TTL)

#define GPIO_LED_PIN 40 // LED verde responsável pela indicação de cada pacote recebido
#define BOT_ERASE 16    // Botão para apagar as configurações de rede e reiniciar a ancora

//      WK-Board // E32_S3-DevKit // LORA32 // E32_WROOM-DevKit
#define TXD2 43 // 17 // 38 // 17
#define RXD2 44 // 18 // 39 // 16
#define RTS2 17 // 8 // 40 // 26
#define CTS2 18 // 3 // 41 // 25

#define BATTERY_SEND_INTERVAL 10000 // Tempo minimo para reenviar o nível de bateria da mesma tag
#define MAX_TRACKED_TAGS 50         // Número máximo de tags armazenada em memória para acumulo de dados
#define MIN_READINGS_TO_PUBLISH 10  // Quantidade mínima de amostras de uma tag para envio normal
#define MIN_INTERVAL_TO_SEND 1000   // Intervalo mínimo entre envios para a mesma tag (em ms)
#define SLOT_TIMEOUT_MS 5000        // Tempo limite para forçar o envio de um slot se não atingir a quantidade mínima de amostras (em ms)