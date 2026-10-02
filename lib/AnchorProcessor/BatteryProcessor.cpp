#include "BatteryProcessor.h"

// Estrutura que controla a memória de cada tag
struct BatteryRecord {
    char tag_id[13];            // 12 caracteres do MacAddress + 1 do terminador nulo '\0'
    unsigned long lastSendTime; // Timestamp do último envio
    bool active;                // Sinaliza se esse slot da memória está em uso
};
BatteryRecord batteryTracker[MAX_TRACKED_TAGS];

BatteryCallback onBatteryPublishCallback = nullptr; // Armazena a função de callback cadastrada no main.cpp

/* @brief Processa um novo valor de bateria recebido de uma tag 
 * @param tag_id ID da tag (MAC address)
 * @param batteryLevel Nível de bateria da tag (de 0 a 100)
 */
void processNewBatteryReading(String tag_id, uint8_t batteryLevel){
    bool need_to_publish = false;   // Flag que sinaliza a necessidade de envio dos dados
    int emptySlot = -1;             // Índice do primeiro slot vazio encontrado

    // Procura se a tag já existe no banco de dados ou procura um slot vazio
    for (int i = 0; i < MAX_TRACKED_TAGS; i++) {
        if (batteryTracker[i].active) {
            if (strcmp(batteryTracker[i].tag_id, tag_id.c_str()) == 0) { // Compara o id da tag armazenada com a recebida
                if (millis() - batteryTracker[i].lastSendTime >= BATTERY_SEND_INTERVAL) {
                    log_d("Tag %s (posição %d) atualizada. Ultimo envio em %dms.\n", tag_id.c_str(), i, batteryTracker[i].lastSendTime);
                    need_to_publish = true;
                    batteryTracker[i].lastSendTime = millis();
                    break;
                } else {
                    log_d("Tag %s (posição %d) descartada (Enviado em %dms)\n", tag_id.c_str(), i, batteryTracker[i].lastSendTime);
                    return;
                }
            } else { // Se a tag não corresponder, analisa se o slot pode ser limpado 
                unsigned long tempoOcioso = millis() - batteryTracker[i].lastSendTime;
                if (tempoOcioso > BATTERY_SEND_INTERVAL) { // A tag armazenada já pode ser excluida. se necessário a nova tag será armazenada neste campo.
                    log_d("Tag muito antiga (%s) encontrada na posição %d (último envio há %d ms). Limpando esse slot.\n", batteryTracker[i].tag_id, i, tempoOcioso);
                    batteryTracker[i].active = 0;

                    // Se a tag atual ainda não encontrou um slot vazio, poderá utilzar esse slot caso não encontre seu próprio registro nos campos restantes
                    if (emptySlot == -1) { 
                        emptySlot = i;
                    }
                }
            }
        } else if (emptySlot == -1) { // Encontrou o primeiro slot livre
            emptySlot = i;
        }
    }

    // Tag não encontrada em nenhum slot e há espaço disponível
    if (need_to_publish == false && emptySlot != -1){ 
        // Armazena os dados na tag no slot disponível
        strlcpy(batteryTracker[emptySlot].tag_id, tag_id.c_str(), sizeof(batteryTracker[emptySlot].tag_id)); // garante que não haverá gravação de uma string maior que 13 bytes.
        batteryTracker[emptySlot].lastSendTime = millis();
        batteryTracker[emptySlot].active = true;

        need_to_publish = true;
        log_d("Tag %s registrada no slot %d (%d%%)\n", tag_id.c_str(), emptySlot, batteryLevel);
    }

    // Enviar o status via MQTT:
    if (need_to_publish){
        if (onBatteryPublishCallback != nullptr){
            log_d("Enviando nível de bateria da tag %s para o callback.", tag_id.c_str());
            onBatteryPublishCallback(tag_id, batteryLevel);
        } else {
            log_e("Função de callback para envio do nível de bateria não cadstrada.");
        }
    }
}

/* @brief Função para cadastrar o Callback de envio dos dados de bateria das tags após tratamento*/
void setBatteryProcessorCallback(BatteryCallback cb) {
    onBatteryPublishCallback = cb;
}