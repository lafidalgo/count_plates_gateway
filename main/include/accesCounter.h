#ifndef ACCES_COUNTER_H
#define ACCES_COUNTER_H

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "memoriaNVS.h"

typedef enum{
    SENSOR_CMD_ACCES = 1,
    SENSOR_CMD_EGRESS,
    SENSOR_CMD_HTBT,
    SENSOR_CMD_OBSTR,
    SENSOR_CMD_INIT,
    SENSOR_CMD_DOBSTR
} ble_cmd_t;

typedef struct{
    ble_cmd_t cmd;
    uint32_t total_entradas;
    uint32_t total_saidas;
    uint8_t battery;
    uint16_t crc;
} ble_payload_t;

typedef struct{
    char header[3];
    char send_addr[2];
    char dest_addr[2];
    char payload[10];
} ble_msg_t;

typedef struct {
    uint32_t old_saidas;
    uint32_t old_entradas;
    int obstrucao;
    int msgs_perdidas;
    uint8_t msgs_repetidas;
    uint8_t erros_crc;
    uint16_t reinicios;
    uint32_t heart_beat;
} hist_mens;

typedef struct {
    hist_mens *historico_mensagens;
} historico_repeticao;

extern historico_repeticao historico;

void pins_init( void );
void uart_init( void );
void ble_config( void );
uint8_t ble_mesh_parse(const char* data);
void uart_event_task(void *pvParameters);
void iniciar_simulador(void *pvParameters);
void jdy_watchdog(void *pvParameters);

#endif