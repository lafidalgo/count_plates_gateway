#ifndef MEMORIANVS_H
#define MEMORIANVS_H

#include <stdio.h>
#include "macros.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "Manipulacao_Arquivos.h"
#include "accesCounter.h"

#define TEMPO_ENVIO 2
#define CMP_AB 3
#define CMP_BA 4
#define BATERIA 5
#define OBSTRUCAO 6
#define MSGS_PERDIDAS 7
#define MSGS_REPETIDAS 8
#define REINICIOS 9
#define HEARTBEAT 10
#define DIGITAL_TWIN 11

SemaphoreHandle_t xSemaphoreAcessoSpiffs;
StaticSemaphore_t xSemaphoreBufferAcessoNVS;

int pegar_ultimo_id_backup(void);
int escrita_nvs_backup(char *, uint32_t, uint32_t, uint8_t, uint8_t, char *);
mensagem_backup_iothub *leitura_nvs_backup(int);
int init_arquivos_csv(void);
int escrita_mensagem_nao_enviada(mensagem_backup_iothub *, int);
int escrita_historico_sensores(void);
void leitura_nvs_historico(void);
void reinicia_salva_historico(void);

#endif