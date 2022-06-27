#ifndef IOTHUB_DEVICETWIN
#define IOTHUB_DEVICETWIN

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <cJSON.h>

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
#include "Manipulacao_Arquivos.h"
#include "atualizacao_ota.h"
#include "pppos_client.h"
#include "application_timers.h"
#include "autenticacao_dps.h"
#include "ethernet_setup.h"
// The protocol you wish to use should be uncommented
//
#define ATIVA 1
#define DESATIVADA 0
#define WIFI 1
#define ETHERNET 2
#define PPPOS 3

#define SAMPLE_MQTT

#ifdef SAMPLE_MQTT
#include "iothubtransportmqtt.h"
#endif // SAMPLE_MQTT

#ifdef SET_TRUSTED_CERT_IN_SAMPLES
#include "certs.h"
#endif // SET_TRUSTED_CERT_IN_SAMPLES

/*
    Variavel para saber qual tipo de conexao esta sendo usada pelo esp
    1 -> wifi
    2 -> ethernet
    3 -> PPPos
 */

extern int tipo_conexao_ativa;

/*
    Variavel para saber se deve trocar o tipo de conexao
    0 -> Nao trocar
    1 -> Trocar conexao
 */

extern int trocar_tipo_conexao;
extern int desativar_iothub;

#define DOWORK_LOOP_NUM 3

typedef struct SENSOR_TAG
{
    char *digitalTwinId;
    char *macAddress;
    uint16_t weightReference; // serve para relacionar o elemento que sofreu atualizacao com o armazenado em tempo de exec
    uint16_t repMeasureQtd;
    uint16_t ulpWakeUpPeriod;
    uint16_t heartbeatPeriod;
} SensorState;

typedef struct rede_configs_tag
{
    char *ip;
    char *gateway;
    char *netmask;
    char *DNS;
    char *ativo;
    char *ssid;
    char *senha;
    char *usuario_gsm;
} rede_configs;

typedef struct myDevice_TAG
{
    char *tipo_conexao; // reported // nome em portugues
    char *versao_firmware;
    char *data_update_firmware;
    char *chave_update_firmware;
    int num_sensores;
    SensorState *sensor; // desired
    char *ota_update_status;
    rede_configs config_rede;
} Device; // mudar para device

typedef struct informacoesOTA_Tag
{
    char *versao;
    char *uri;

} informacoesOTA;

typedef struct
{
    uint16_t crash;
    uint16_t watchdog;
    uint16_t software;
    uint16_t inicioNormal;
} reset_reasons;

// Device global para recuperar em tempo de execucao
extern Device *deviceTwinTempoExecucao; // vai representar o valor das propriedades
extern Device device_alocado;
extern IOTHUB_DEVICE_CLIENT_LL_HANDLE iotHubClientHandle; // handle para as funcoes
// Semaforo para a verificacao da estrutura armazenando as caracteristicas em tempoo de execucao
SemaphoreHandle_t xSemaphoreDeviceTwin;
StaticSemaphore_t xSemaphoreBufferDeviceTwin;
SemaphoreHandle_t xSemaphoreAcessoInternet;
StaticSemaphore_t xSemaphoreBufferOtaUpdate;

extern int status_update_device_twin;
extern char *rede_ip;
extern char *rede_gateway;
extern char *rede_netmask;
extern char *rede_dns;
extern char *cert_common_name;
extern char *id_scope;
// Prototipos de funcoes
char *serializeToJson(Device *);
Device *parseFromJson(const char *, DEVICE_TWIN_UPDATE_STATE);
informacoesOTA *parseUpdateFromJson(const char *json);
int deviceMethodCallback(const char *, const unsigned char *, size_t, unsigned char **, size_t *, void *);
void deviceTwinCallback(DEVICE_TWIN_UPDATE_STATE, const unsigned char *, size_t, void *);
void reportedStateCallback(int, void *);
void InicializarIothubDeviceTwin(uint8_t, char *, char *);
int verifica_status_device_twin(void);
void reporta_status_ota(char *);
void desativar_conexao_iothub(void);
void trocar_conexao(int);
void reporta_status_configs_rede(void);
int verifica_parametros_rede(char *, char *, char *, char *);
void troca_parametros_rede(void);
int escreve_configuracao_rede_em_memoria(void);
#endif