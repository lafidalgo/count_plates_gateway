// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef IOTHUB_CLIENT_SAMPLE_MQTT_H
#define IOTHUB_CLIENT_SAMPLE_MQTT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <cJSON.h>
#include "iothub_client.h"
#include "iothub_device_client_ll.h"
#include "iothub_client_options.h"
#include "iothub_message.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/shared_util_options.h"
#include "iothubtransportmqtt.h"
#include "iothub_client_options.h"
#include "esp_system.h"
#include "freertos/task.h"
#include "Manipulacao_Arquivos.h"
#include "Iothub_DeviceTwin.h"
#include "wifi_setup.h"
#include "autenticacao_dps.h"
#include "Iothub_DeviceTwin.h"

extern const char *TAGmsgIot;
extern int contador_mensagens_perdidas;
extern int8_t json_watchdog_counter;   // 15min para watchdog
/* Struct para manter uma referencia das mensagens enviadas */

#define NUM_MENS_MAX 10

typedef struct mensagem_envio_tag {
    int trigger_count;  // valor do par NVS
    int sensorName; // chave do par NVS
} Mensagem_Envio_Iothub;

typedef struct{
    uint32_t batidas_a_b;
    uint32_t batidas_b_a; 
    uint8_t nivel_bateria;
    uint8_t obstrucao;
    int msgs_perdidas;
    uint8_t msgs_repetidas;
    uint16_t reinicios;
    uint32_t heart_beat;
    char *digital_twin_id; 
    char *tempo_envio;
} mensagem_backup_iothub;

typedef struct 
{
    int quantidade;
    mensagem_backup_iothub *mensagens_recebidas;
} objeto_mensagem_ram;

/* Fila para mensagens quando a conexao cair */
xQueueHandle Fila_Batidas_Acumuladas;

/* flag para verificar o callback do status da conexao */
int IothubMensagemFlag;
/* FreeRTOS event group to signal when we are connected & ready to make a request */
int montar_mensagem_iothub(mensagem_backup_iothub *, int, int, char *);
bool Enviar_Mensagem_Iothub(mensagem_backup_iothub *, int, int);
bool iniciar_mensagem_recebida(void);
bool adicionar_mensagem_recebida(char*, uint32_t, uint32_t, uint8_t, uint8_t, int, uint8_t, uint16_t, uint32_t, char*);
bool limpar_mensagens_recebidas(void);
objeto_mensagem_ram *pegar_mensagens(void);

void SendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT , void*);
void connection_status_callback(IOTHUB_CLIENT_CONNECTION_STATUS , IOTHUB_CLIENT_CONNECTION_STATUS_REASON , void* );
IOTHUBMESSAGE_DISPOSITION_RESULT ReceiveMessageCallback(IOTHUB_MESSAGE_HANDLE , void* );

#ifdef __cplusplus
}
#endif

#endif /* IOTHUB_CLIENT_SAMPLE_MQTT_H */
