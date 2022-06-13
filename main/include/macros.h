#ifndef MACROS_H
#define MACROS_H

#include "time.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "Iothub_DeviceTwin.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "azure_macro_utils/macro_utils.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/platform.h"
#include "iothub_device_client.h"
#include "iothub_client_options.h"
#include "iothub.h"
#include "iothub_message.h"
#include "parson.h"
#include "sdkconfig.h"
#include "wifi_setup.h"
#include "esp_spiffs.h"

/*
typedef struct MSG_MESH {
    uint8_t obstrucao;
    uint8_t batidas_a_b;
    uint8_t batidas_b_a;
    uint8_t nivel_bateria;
    uint16_t endereco_sensor;
} mensagem_mesh;

typedef struct BLE_MES_R {
    char *cabecalho;
    char *ble_ender_que_envia;
    char *ble_ender_que_recebe;
    mensagem_mesh mensagem_sensor;
} ble_message_received;

typedef struct BLE_MES_S {
    char *cabecalho;
    char *endereco_que_envia;
    char *endereco_que_recebe;
    mensagem_mesh mensagem_sensor;
} ble_message_to_send;
*/

#define char_separator (char*)","

uint16_t get_bytes_address(char *);
char* hex_to_string(char *);
// char[8] get_string_address(int *)
int is_str_equal(char *, char *);
int is_str_true(char *);
int is_str_false(char *);
int is_str_bool(char *);
int achar_indice_sensor_por_endereco(uint16_t);
int are_float_equal(float, float);
int escrever_arquivo(char *, FILE *);
char *parse_field(char *, int, const char *);
int string_to_int(char *);
FILE *abrir_arquivo_e_montar_particao(char *, char *, char *);
int fechar_arquivo(char *, FILE *);
int get_size_csv_line(FILE *);
int montar_particao(char *);
char *pegar_string_tempo_agora(void);
int criar_arquivo_backup(char *, char *);
reset_reasons pegar_motivo_crash(reset_reasons);
char *baixar_arquivo(char *);

#endif