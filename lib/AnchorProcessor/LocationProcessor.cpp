#include "LocationProcessor.h"

// Configurações
uint16_t readingsToPublish = MIN_READINGS_TO_PUBLISH;   // Quantidade mínima de amostras de uma tag para envio normal
uint16_t minIntervalToSend = MIN_INTERVAL_TO_SEND;      // Intervalo mínimo entre envios para a mesma tag (em ms)
uint16_t slotTimeoutMs = SLOT_TIMEOUT_MS;               // Tempo limite para forçar o envio de um slot se não atingir a quantidade mínima de amostras (em ms)

// Estrutura para armazenar os dados de cada tag
struct TagRecord {
    //unsigned long last_seen;      // Timestamp da última leitura recebida para a tag
    unsigned long batch_start_time; // Timestamp da primeira amostra do lote atual para a tag
    String tag_id;                  // ID da tag (MAC address)
    float filtered_azimuth;         // Azimute filtrado
    float min_azimuth;              // Valor mais baixo do azimute registrado no lote
    float max_azimuth;              // Valor mais alto do azimute registrado no lote
    float filtered_elevation;       // Elevação filtrada
    float min_elevation;            // Valor mais baixo da elevação registrado no lote
    float max_elevation;            // Valor mais alto da elevação registrado no lote
    float filtered_rssi;            // RSSI filtrado
    int reading_count;              // Contador de leituras acumuladas para a tag
    bool active;                    // Indica se o índice está ativo (em uso)
};
TagRecord tag_database[MAX_TRACKED_TAGS];

SemaphoreHandle_t tagMutex; // Mutex para proteger o acesso ao banco de dados de tags

LocationCallback onLocationPublishCallback = nullptr; // Armazena a função de callback cadastrada no main.cpp

/* @brief Filtra um valor usando uma média móvel
 * @param current_filtered Valor filtrado atual
 * @param new_raw Nova leitura bruta
 * @param readings Quantidade de leituras acumuladas até o momento
 * @return Novo valor filtrado
 */
float filterValue(float current_filtered, float new_raw, int readings) {
    float result = (current_filtered * readings) + new_raw;
    result /= (readings + 1);
    return result;
}

/* @brief Limpa os dados de um índice específico no banco de dados
 * @param index Índice do banco de dados a ser limpo
 */
void clearDatabaseIndex(int index) {
    log_v("Limpando dados da tag %s no índice %d", tag_database[index].tag_id.c_str(), index);
    if (index >= 0 && index < MAX_TRACKED_TAGS) {
        memset(&tag_database[index], 0, sizeof(TagRecord));
    }
}

/* @brief Processa uma nova leitura de localização de uma tag
 * @param id ID da tag (MAC address)
 * @param raw_azimuth Azimute bruto da leitura
 * @param raw_elevation Elevação bruta da leitura
 * @param rssi Intensidade do sinal recebido (RSSI)
 */
void processNewLocationReading(String id, float raw_azimuth, float raw_elevation, float rssi) {
    if (xSemaphoreTake(tagMutex, portMAX_DELAY) == pdTRUE) {
        int empty_slot = -1; // Índice do primeiro slot vazio encontrado

        // Procura se a tag já existe no banco de dados ou procura um slot vazio
        for (int i = 0; i < MAX_TRACKED_TAGS; i++) {
            if (tag_database[i].active && tag_database[i].tag_id == id) {
                
                // Atualiza os valores filtrados e os limites de azimute e elevação
                tag_database[i].filtered_azimuth = filterValue(tag_database[i].filtered_azimuth, raw_azimuth, tag_database[i].reading_count);
                tag_database[i].filtered_elevation = filterValue(tag_database[i].filtered_elevation, raw_elevation, tag_database[i].reading_count);
                tag_database[i].filtered_rssi = filterValue(tag_database[i].filtered_rssi, rssi, tag_database[i].reading_count);
                if (raw_azimuth < tag_database[i].min_azimuth) tag_database[i].min_azimuth = raw_azimuth;
                if (raw_azimuth > tag_database[i].max_azimuth) tag_database[i].max_azimuth = raw_azimuth;
                if (raw_elevation < tag_database[i].min_elevation) tag_database[i].min_elevation = raw_elevation;
                if (raw_elevation > tag_database[i].max_elevation) tag_database[i].max_elevation = raw_elevation;

                tag_database[i].reading_count++;
                //tag_database[i].last_seen = millis();

                xSemaphoreGive(tagMutex);
                return;
            }
            if (!tag_database[i].active && empty_slot == -1) { // Marca o primeiro slot vazio encontrado
                empty_slot = i;
            }
        }

        // Registra nova tag no primeiro slot vazio encontrado
        if (empty_slot != -1) {
            tag_database[empty_slot].tag_id = id;
            tag_database[empty_slot].filtered_azimuth = raw_azimuth;
            tag_database[empty_slot].filtered_elevation = raw_elevation;
            tag_database[empty_slot].min_azimuth = raw_azimuth;
            tag_database[empty_slot].max_azimuth = raw_azimuth;
            tag_database[empty_slot].min_elevation = raw_elevation;
            tag_database[empty_slot].max_elevation = raw_elevation;
            tag_database[empty_slot].filtered_rssi = rssi;
            tag_database[empty_slot].reading_count = 1;
            tag_database[empty_slot].batch_start_time = millis();
            //tag_database[empty_slot].last_seen = millis();
            tag_database[empty_slot].active = true;
        } else {
            log_e("Aviso: Banco de dados de tags cheio. Nova tag %s não registrada.", id.c_str());
        }
        xSemaphoreGive(tagMutex);
    }
}

/* @brief Task para envio, limpeza e verificação de timeout de cada slot*/
void TagsSender(void *pvParameters) {
    while (true) {        
        // Avalia uma tag por vez para travar o Mutex pelo menor tempo possível
        for (int i = 0; i < MAX_TRACKED_TAGS; i++) {
            bool need_to_publish = false; // Flag que sinaliza a necessidade de envio dos dados
            String pub_id; float pub_az, pub_el, pub_rssi, pub_min_az, pub_max_az, pub_min_el, pub_max_el; uint8_t pub_samples;

            if (tag_database[i].active && xSemaphoreTake(tagMutex, portMAX_DELAY) == pdTRUE) {
                // Checa se o slot deve ser enviado ((Envia se atingiu o tempo minimo de espera e a quantidade minima de amostras) ou se atingiu o tempo limite do lote)
                if ((tag_database[i].reading_count >= readingsToPublish && millis() - tag_database[i].batch_start_time >= minIntervalToSend) || millis() - tag_database[i].batch_start_time >= slotTimeoutMs) {
                    need_to_publish = true;
                    pub_id = tag_database[i].tag_id;
                    pub_az = tag_database[i].filtered_azimuth;
                    pub_el = tag_database[i].filtered_elevation;
                    pub_rssi = tag_database[i].filtered_rssi;
                    pub_min_az = tag_database[i].min_azimuth;
                    pub_max_az = tag_database[i].max_azimuth;
                    pub_min_el = tag_database[i].min_elevation;
                    pub_max_el = tag_database[i].max_elevation;
                    pub_samples = tag_database[i].reading_count;
                    clearDatabaseIndex(i); // Limpa o slot após o envio para liberar espaço para novas tags
                }
                xSemaphoreGive(tagMutex);
            }

            // Executa o envio fora do Mutex para não bloquear a coleta de novas leituras
            if (need_to_publish) {
                if (onLocationPublishCallback != nullptr){
                    log_i("Enviando dados da tag %s (ID: %d) para o callback com %d amostras.", pub_id.c_str(), i, pub_samples);
                    onLocationPublishCallback(pub_id, pub_az, pub_el, pub_rssi, pub_min_az, pub_max_az, pub_min_el, pub_max_el, pub_samples);
                } else {
                    log_e("Função de callback para envio dos dados de localização não cadstrada.");
                }
            } 
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // Aguarda para realizar uma nova checagem:
    }
}

/* @brief Inicializa o sistema de tratamento dos dados de localização */
void setupLocationProcessor() {
    tagMutex = xSemaphoreCreateMutex();
    for(int i = 0; i < MAX_TRACKED_TAGS; i++) {
        tag_database[i].active = false;
    }
    // Inicia a task de envio em paralelo no Core 0 (Deixando o Core 1 livre)
    xTaskCreatePinnedToCore(TagsSender, "TagsSender", 4096, NULL, 1, NULL, 0);
}

/* @brief Configura os parâmetros de processamento e retenção de envio dos dados de localização
 * @param ReadingsToPublish Quantidade mínima de amostras de uma tag para envio normal
 * @param MinIntervalToSend Intervalo mínimo entre envios para a mesma tag (em ms)
 * @param SlotTimeoutMs Tempo limite para forçar o envio de um slot se não atingir a quantidade mínima de amostras (em ms)
 * @return void
 */
void configLocationProcessor(uint16_t ReadingsToPublish, uint16_t MinIntervalToSend, uint16_t SlotTimeoutMs){
    //Salva os parâmetros recebidos em variáveis globais para uso interno
    readingsToPublish = ReadingsToPublish;
    minIntervalToSend = MinIntervalToSend;
    slotTimeoutMs = SlotTimeoutMs;
}

/* @brief Função para cadastrar o Callback de envio dos dados de localização das tags após tratamento*/
void setLocationProcessorCallback(LocationCallback cb) {
    onLocationPublishCallback = cb;
}