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

#define TEMPO_ENVIO 2
#define TYPE 3
#define WEIGHT 4
#define QUANTITY 5
#define VOLTAGE 6
#define DIGITAL_TWIN 7

SemaphoreHandle_t xSemaphoreAcessoSpiffs;
StaticSemaphore_t xSemaphoreBufferAcessoNVS;

int pegar_ultimo_id_backup(void);
int escrita_nvs_backup(char *, uint32_t, uint32_t, uint8_t, uint8_t, char *);
mensagem_backup_iothub *leitura_nvs_backup(int);
int init_arquivos_csv(void);
int escrita_mensagem_nao_enviada(mensagem_backup_iothub *, int);
int escrita_historico_sensores(void);
void reinicia_salva_historico(void);

#endif