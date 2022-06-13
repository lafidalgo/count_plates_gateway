#include "Iothub_DeviceTwin.h"
#include "pppos_client.h"
#include <string.h>
#include "macros.h"
#include "accesCounter.h"
#include "esp_task_wdt.h"

#define WIFI                        1
#define ETHERNET                    2
#define PPPOS                       3
#define TROCA_WIFI_ETH              1
#define TROCA_ETH_WIFI              2
#define TROCA_PPPOS_WIFI            3
#define TROCA_WIFI_PPPOS            4
#define TROCA_WIFI_CONFIG           5
#define MANTEM_REDE                 0

#define FLAG_OTA_ATT                "atualizacao_ota_orion_esp"
#define FLAG_RESET                  "reset_orion_esp"
#define FLAG_CERT_ATT               "atualizacao_certificado_esp"
#define FLAG_TROCA                  "troca_conexao_orion_esp"

#define TAMANHO_STRING_REDE         256
#define TAMANHO_MENSAGEM            60

int status_update_device_twin = 0;
// variavel para tratar a conexao ativa no momento
int tipo_conexao_ativa;
int desativar_iothub = 0;
IOTHUB_CLIENT_STATUS status;
//char * eth_ip;
//char * eth_gateway;
//char * eth_netmask;
int ethernet_ja_inicializado;
// Handle para a comunicacao com o IotHub
IOTHUB_DEVICE_CLIENT_LL_HANDLE iotHubClientHandle;
int trocar_tipo_conexao = 0;
int flag_verificar_troca_conexao = 0;
int flag_trocar_tipo_conexao = 0;
int estatico_ativo;
int esp_orion_reset_status;
reset_reasons reset_status;

char *serializeToJson(Device *device)
{
    reset_status = pegar_motivo_crash(reset_status);
    char *result;

    cJSON *root_object = cJSON_CreateObject();
    cJSON *rede_object;
    cJSON *crash_object;
    cJSON *array_object;

    // Only reported properties:
    cJSON_AddStringToObject(root_object, "tipo_conexao", device->tipo_conexao);
    cJSON_AddStringToObject(root_object, "versao_firmware", device->versao_firmware);
    cJSON_AddStringToObject(root_object, "data_update_firmware", device->versao_firmware);
    cJSON_AddStringToObject(root_object, "chave_update_firmware", device->versao_firmware);
    cJSON_AddStringToObject(root_object, "ota_update_status", device->ota_update_status);
    cJSON_AddNumberToObject(root_object, "num_sensores", device->num_sensores);
    cJSON_AddNumberToObject(root_object, "baud_rate_gateway", device->baud_rate_gateway);
    cJSON_AddStringToObject(root_object, "ble_address_gateway", device->ble_address_gateway);
    cJSON_AddStringToObject(root_object, "net_id_gateway", device->net_id_gateway);

    cJSON_AddItemToObject(root_object, "quantidade_resets", crash_object = cJSON_CreateObject());
    cJSON_AddNumberToObject(crash_object, "crash", reset_status.crash);
    cJSON_AddNumberToObject(crash_object, "software", reset_status.software);
    cJSON_AddNumberToObject(crash_object, "watchdog", reset_status.watchdog);
    cJSON_AddNumberToObject(crash_object, "inicioNormal", reset_status.inicioNormal);

    cJSON_AddItemToObject(root_object, "config_rede", rede_object = cJSON_CreateObject());
    cJSON_AddStringToObject(rede_object, "ip", device->config_rede.ip);
    cJSON_AddStringToObject(rede_object, "gateway", device->config_rede.gateway);
    cJSON_AddStringToObject(rede_object, "netmask", device->config_rede.netmask);
    cJSON_AddStringToObject(rede_object, "DNS", device->config_rede.DNS);
    cJSON_AddStringToObject(rede_object, "ativo", device->config_rede.ativo);
    cJSON_AddStringToObject(rede_object, "nome_rede", device->config_rede.ssid);
    cJSON_AddStringToObject(rede_object, "senha_rede", device->config_rede.senha);
    cJSON_AddStringToObject(rede_object, "usuario_gsm", device->config_rede.usuario_gsm);
    
    int contador;
    // Atualizar todos os sensor's
    array_object = cJSON_AddArrayToObject(root_object, "sensores");

    for (contador = 0; contador < device->num_sensores; contador++)
    {
        cJSON *sensor_object = cJSON_CreateObject();

        cJSON_AddStringToObject(sensor_object, "digitalTwinId", device->sensor[contador].digitalTwinId);
        cJSON_AddNumberToObject(sensor_object, "bleMeshAddr", device->sensor[contador].bleMeshAddr);

        cJSON_AddItemToArray(array_object, sensor_object);
    }

    result = cJSON_PrintUnformatted(root_object);
    cJSON_Delete(root_object);

    return result;
}

//  Converts the desired properties of the Device Twin JSON blob received from IoT Hub into a Car object.
Device *parseFromJson(const char *json, DEVICE_TWIN_UPDATE_STATE update_state)
{
    Device *device = malloc(sizeof(Device));
    cJSON *root_object;
    int k;

    if (NULL == device)
    {
        (void)printf("ERROR: Failed to allocate memory\r\n");
    }

    else
    {
        (void)memset(device, 0, sizeof(Device));

        root_object = cJSON_Parse(json);
        cJSON *read_object;
        
        // Only desired properties:
        int num_sensores;
        uint32_t baud_rate;
        char *tipo_conexao = (char *)malloc( sizeof(char)*TAMANHO_MENSAGEM );
        char *versao_firmware = (char *)malloc( sizeof(char)*TAMANHO_MENSAGEM );
        char *data_update_firmware = (char *)malloc( sizeof(char)*TAMANHO_MENSAGEM );
        char *chave_update_firmware = (char *)malloc( sizeof(char)*TAMANHO_MENSAGEM );
        char *ip = (char *)malloc( sizeof(char)*TAMANHO_MENSAGEM );
        char *DNS = (char *)malloc( sizeof(char)*TAMANHO_MENSAGEM );
        char *ativo = (char *)malloc( sizeof(char)*TAMANHO_MENSAGEM );
        char *gateway = (char *)malloc( sizeof(char)*TAMANHO_MENSAGEM );
        char *netmask = (char *)malloc( sizeof(char)*TAMANHO_MENSAGEM );
        char *ssid = (char *)malloc( sizeof(char)*TAMANHO_MENSAGEM );
        char *senha = (char *)malloc( sizeof(char)*TAMANHO_MENSAGEM );
        char *usuario_gsm = (char *)malloc( sizeof(char)*TAMANHO_MENSAGEM );
        char *gateway_address = (char *)malloc( sizeof(char)*TAMANHO_MENSAGEM );
        char *net_id_gateway = (char *)malloc( sizeof(char)*TAMANHO_MENSAGEM );

        if (update_state == DEVICE_TWIN_UPDATE_COMPLETE)
        {
            read_object = cJSON_GetObjectItem(root_object, "desired");
        }
        else
        {
            read_object = root_object;
        }

        sprintf(gateway_address, cJSON_GetObjectItem(read_object, "ble_address_gateway")->valuestring);
        sprintf(tipo_conexao, cJSON_GetObjectItem(read_object, "tipo_conexao")->valuestring);
        sprintf(versao_firmware, cJSON_GetObjectItem(read_object, "versao_firmware")->valuestring);
        sprintf(data_update_firmware, cJSON_GetObjectItem(read_object, "data_update_firmware")->valuestring);
        sprintf(chave_update_firmware, cJSON_GetObjectItem(read_object, "chave_update_firmware")->valuestring);
        sprintf(net_id_gateway, cJSON_GetObjectItem(read_object, "net_id_gateway")->valuestring);
        num_sensores = cJSON_GetObjectItem(read_object, "num_sensores")->valueint;
        baud_rate = cJSON_GetObjectItem(read_object, "baud_rate_gateway")->valueint;

        cJSON *reset_object = cJSON_GetObjectItem(read_object, "quantidade_resets");
        reset_status.crash = cJSON_GetObjectItem(reset_object, "crash")->valueint;
        reset_status.watchdog = cJSON_GetObjectItem(reset_object, "watchdog")->valueint;
        reset_status.software = cJSON_GetObjectItem(reset_object, "software")->valueint;
        reset_status.inicioNormal = cJSON_GetObjectItem(reset_object, "inicioNormal")->valueint;

        cJSON *rede_object = cJSON_GetObjectItem(read_object, "config_rede");
        sprintf(ip, cJSON_GetObjectItem(rede_object, "ip")->valuestring);
        sprintf(DNS, cJSON_GetObjectItem(rede_object, "DNS")->valuestring);
        sprintf(ativo, cJSON_GetObjectItem(rede_object, "ativo")->valuestring);
        sprintf(gateway, cJSON_GetObjectItem(rede_object, "gateway")->valuestring);
        sprintf(netmask, cJSON_GetObjectItem(rede_object, "netmask")->valuestring);
        sprintf(ssid, cJSON_GetObjectItem(rede_object, "nome_rede")->valuestring);
        sprintf(senha, cJSON_GetObjectItem(rede_object, "senha_rede")->valuestring);
        sprintf(usuario_gsm, cJSON_GetObjectItem(rede_object, "usuario_gsm")->valuestring);

        device->num_sensores = num_sensores;
        device->baud_rate_gateway = baud_rate;

        if (gateway_address != NULL)
        {
            device->ble_address_gateway = (char *)malloc( 1 + strlen(gateway_address) );
            if (NULL != device->ble_address_gateway)
            {
                (void)strcpy(device->ble_address_gateway, gateway_address);
            }
        }

        if (net_id_gateway != NULL)
        {
            device->net_id_gateway = (char *)malloc( 1 + strlen(net_id_gateway) );
            if (NULL != device->net_id_gateway)
            {
                (void)strcpy(device->net_id_gateway, net_id_gateway);
            }
        }

        if (tipo_conexao != NULL)
        {
            device->tipo_conexao = (char *)malloc( 1 + strlen(tipo_conexao) );
            if (NULL != device->tipo_conexao)
            {
                (void)strcpy(device->tipo_conexao, tipo_conexao);
            }
        }

        if (versao_firmware != NULL)
        {
            device->versao_firmware = (char *)malloc( 1 + strlen(versao_firmware) );
            if (NULL != device->versao_firmware)
            {
                (void)strcpy(device->versao_firmware, versao_firmware);
            }
        }

        if (data_update_firmware != NULL)
        {
            device->data_update_firmware = (char *)malloc( 1 + strlen(data_update_firmware) );
            if (NULL != device->data_update_firmware)
            {
                (void)strcpy(device->data_update_firmware, data_update_firmware);
            }
        }

        if (chave_update_firmware != NULL)
        {
            device->chave_update_firmware = (char *)malloc( 1 + strlen(chave_update_firmware) );
            if (NULL != device->chave_update_firmware)
            {
                (void)strcpy(device->chave_update_firmware, chave_update_firmware);
            }
        }
        // se a conexao ethernet estiver ativa reportar as configuracoes que estao ativas
        

        if (ip != NULL)
        {
            device->config_rede.ip = (char *)malloc( 1 + strlen(ip) );
            if (NULL != device->config_rede.ip)
            {
                (void)strcpy(device->config_rede.ip, ip);
            }
        }

        if (DNS != NULL)
        {
            device->config_rede.DNS = (char *)malloc( 1 + strlen(DNS) );
            if (NULL != device->config_rede.DNS)
            {
                (void)strcpy(device->config_rede.DNS, DNS);
            }
        }

        if (ativo != NULL)
        {
            device->config_rede.ativo = (char *)malloc( 1 + strlen(ativo) );
            if (NULL != device->config_rede.ativo)
            {
                (void)strcpy(device->config_rede.ativo, ativo);
            }
        }

        if (gateway != NULL)
        {
            device->config_rede.gateway = (char *)malloc( 1 + strlen(gateway) );
            if (NULL != device->config_rede.gateway)
            {
                (void)strcpy(device->config_rede.gateway, gateway);
            }
        }

        if (netmask != NULL)
        {
            device->config_rede.netmask = (char *)malloc( 1 + strlen(netmask) );
            if (NULL != device->config_rede.netmask)
            {
                (void)strcpy(device->config_rede.netmask, netmask);
            }
        }
        
        if (usuario_gsm != NULL)
        {
            device->config_rede.usuario_gsm = (char *)malloc( 1 + strlen(usuario_gsm) );
            if (NULL != device->config_rede.usuario_gsm)
            {
                (void)strcpy(device->config_rede.usuario_gsm, usuario_gsm);
            }
        }

        if (ssid != NULL)
        {
            device->config_rede.ssid = (char *)malloc( 1 + strlen(ssid) );
            if (NULL != device->config_rede.ssid)
            {
                (void)strcpy(device->config_rede.ssid, ssid);
            }
        }

        if (senha != NULL)
        {
            device->config_rede.senha = (char *)malloc( 1 + strlen(senha) );
            if (NULL != device->config_rede.senha)
            {
                (void)strcpy(device->config_rede.senha, senha);
            }
        }
        

        //####################################################################
        char *bleMeshAddr_json = (char *)malloc(TAMANHO_MENSAGEM * sizeof(char));
        char *digitalTwinId_json = (char *)malloc(TAMANHO_MENSAGEM * sizeof(char));
        device->sensor = (SensorState *)malloc( sizeof(SensorState)*num_sensores );

        k = 0;
        cJSON *sensores_object = cJSON_GetObjectItem(read_object, "sensores");
        cJSON *sensor_atual;
        cJSON_ArrayForEach(sensor_atual, sensores_object)
        {
            sprintf(bleMeshAddr_json, cJSON_GetObjectItem(sensor_atual, "bleMeshAddr")->valuestring);
            sprintf(digitalTwinId_json, cJSON_GetObjectItem(sensor_atual, "digitalTwinId")->valuestring);

            if (bleMeshAddr_json != NULL)
            {
                device->sensor[k].bleMeshAddr = get_bytes_address(bleMeshAddr_json);
            }

            if (digitalTwinId_json != NULL)
            {
                device->sensor[k].digitalTwinId = (char *)malloc( 1 + strlen(digitalTwinId_json) );
                if (NULL != device->sensor[k].digitalTwinId)
                {
                    (void)strcpy(device->sensor[k].digitalTwinId, digitalTwinId_json);
                }
            }

            k++;
        } // end for

        free(bleMeshAddr_json);
        free(digitalTwinId_json);
        free(tipo_conexao);
        free(versao_firmware);
        free(data_update_firmware);
        free(chave_update_firmware);
        free(ip);
        free(DNS);
        free(ativo);
        free(gateway);
        free(netmask);
        free(ssid);
        free(senha);
        free(gateway_address);
        free(usuario_gsm);
        free(net_id_gateway);
        // ####################################################################################################################

        cJSON_Delete(root_object);
    }

    return device;
}

informacoesOTA *parseUpdateFromJson(const char *json)

{
    informacoesOTA *update = malloc(sizeof(informacoesOTA));
    JSON_Value *root_value = NULL;
    JSON_Object *root_object = NULL;

    if (NULL == update)
    {
        ESP_LOGI("DEVICE_TWIN", "ERROR: Failed to allocate memory");
    }

    else
    {
        (void)memset(update, 0, sizeof(informacoesOTA));
        root_value = json_parse_string(json);
        root_object = json_value_get_object(root_value);

        // Only desired properties:
        JSON_Value *versionNumber;
        JSON_Value *versionUri;
        versionNumber = json_object_dotget_value(root_object, "informacoesOTA.versao");
        versionUri = json_object_dotget_value(root_object, "informacoesOTA.uri");

        if (versionNumber != NULL)
        {
            const char *dataNumber = json_value_get_string(versionNumber);

            if (dataNumber != NULL)
            {
                update->versao = malloc(strlen(dataNumber) + 1);
                if (NULL != update->versao)
                {
                    (void)strcpy(update->versao, dataNumber);
                }
            }
        }
        
        if (versionUri != NULL)
        {
            const char *data = json_value_get_string(versionUri);

            if (data != NULL)
            {
                update->uri = malloc(strlen(data) + 1);
                if (NULL != update->uri)
                {
                    (void)strcpy(update->uri, data);
                }
            }
        }
        json_value_free(root_value);
    }

    return update;
}

int deviceMethodCallback(const char *method_name, const unsigned char *payload, size_t size, unsigned char **response, size_t *response_size, void *userContextCallback)
{
    (void)userContextCallback;
    (void)size;
    /* criar metodo e testar de piscar o led*/
    ESP_LOGI("DIRECT_METHOD", "Method Name: %s", method_name);
    int result = 0;
    int verificador = 0;
    char strftime_buf[64];
    char horario[240];
    time_t now;
    struct tm timeinfo;
    informacoesOTA *parametro = parseUpdateFromJson((const char *)payload);
    if ( is_str_equal(FLAG_OTA_ATT, (char*) method_name) )
    {
        /* existe a string contendo a url para download */
        ESP_LOGI("DIRECT_METHOD", "Inicializando a funcao para OTA");
        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        sprintf(horario, "Horario de inicio de Download da imagem: %s\n", strftime_buf);
        printf("%s", horario);
        printf("Parando timers!\n");
        timer_pause(TIMER_GROUP_0, TIMER_1);
        timer_pause(TIMER_GROUP_0, TIMER_1);

        printf("Bloquando acesso a memoria SPIFF's\n");
        if (xSemaphoreTake(xSemaphoreAcessoSpiffs, (TickType_t)1000) == pdTRUE)
        {
        /* Deve bloquear o acesso a memoria flash enquanto o OTA esta escrevendo nela
            * Para isso bloqueia o acesso a memoria SPIFFS e NVS por parte das outras threads
            */
            printf("Bloquando acesso a memoria NVS\n");
            if (xSemaphoreTake(xSemaphoreAcessoInternet, (TickType_t)1000) == pdTRUE)
            {
                printf("Bloquando acesso a Internet por outras funcoes\n");
                esp_task_wdt_init(600, true); //Inicia o Task WDT com 1min
                verificador = inicia_atualizacao_ota(parametro->uri);
                ESP_LOGW("DIRECT_METHOD", "TERMINOU OTA: %d", verificador);
                xSemaphoreGive(xSemaphoreAcessoInternet);
            }
        }

        const char deviceMethodResponse[] = "{ \"Response\": \"Update Falhou! Algum semaforo para o update estava ocupado!\" }";
        *response_size = sizeof(deviceMethodResponse) - 1;
        *response = malloc(*response_size);
        if (*response != NULL)
        {
            (void)memcpy(*response, deviceMethodResponse, *response_size);
            result = 200;
        }
        else
        {
            result = -1;
        }
        if (verificador == -1)
        {
            xSemaphoreGive(xSemaphoreAcessoSpiffs);
            // reativa as interrupcoes
            //TODO: certificar-se que o device twin não atualzia o PInused de entrada ou saida. Eles já estão fixos nos defines
            /*
            for(int p=0; p<NUM_SENSORES; p++) {
                if( strcmp(deviceTwinTempoExecucao->sensor[p].ativo, "true") == 0 )
                    ativa_interrupcao(deviceTwinTempoExecucao->sensor[p].pinUsed_entrada);
                if( strcmp(deviceTwinTempoExecucao->sensor[p].contagem_saida, "true") == 0 )
                    ativa_interrupcao(deviceTwinTempoExecucao->sensor[p].pinUsed_saida);
            }
            */
            ESP_LOGI("DIRECT_METHOD", "Reabilitando timers!");
            timer_start(TIMER_GROUP_0, TIMER_1);
            timer_start(TIMER_GROUP_0, TIMER_1);
            const char deviceMethodResponse[] = "{ \"Response\": \"Update Falhou! Verifique a URL\" }";
            *response_size = sizeof(deviceMethodResponse) - 1;
            *response = malloc(*response_size);
            if (*response != NULL)
            {
                (void)memcpy(*response, deviceMethodResponse, *response_size);
                result = 200;
            }
            else
            {
                result = -1;
            }
        }
        else if(verificador == 1)
        {
            esp_orion_reset_status = 1;
            const char deviceMethodResponse[] = "{ \"Response\": \"Update realizado!\" }";
            *response_size = sizeof(deviceMethodResponse) - 1;
            *response = malloc(*response_size);
            if (*response != NULL)
            {
                (void)memcpy(*response, deviceMethodResponse, *response_size);
                result = 200;
            }
            else
            {
                result = -1;
            }
        }
        else if(verificador == 2)
        {
            esp_orion_reset_status = 1;
            const char deviceMethodResponse[] = "{ \"Response\": \"Versao atual igual a nova!\" }";
            *response_size = sizeof(deviceMethodResponse) - 1;
            *response = malloc(*response_size);
            if (*response != NULL)
            {
                (void)memcpy(*response, deviceMethodResponse, *response_size);
                result = 200;
            }
            else
            {
                result = -1;
            }
        }
    }
    else if ( is_str_equal(FLAG_RESET, (char*) method_name) )
    {
        const char deviceMethodResponse[] = "{ \"Response\": \"Reset realizado!\" }";
        *response_size = sizeof(deviceMethodResponse)-1;
        *response = malloc(*response_size);
        if (*response != NULL)
        {
            (void)memcpy(*response, deviceMethodResponse, *response_size);
            result = 200;
        }
        else
        {
            result = -1;
        }
        // atualiza flag de reset
        esp_orion_reset_status = 1;
    }
    else if ( is_str_equal(FLAG_CERT_ATT, (char*) method_name) )
    {
        ESP_LOGI("DIRECT_METHOD", "Bloquando acesso a memoria SPIFF's");
        if (xSemaphoreTake(xSemaphoreAcessoSpiffs, (TickType_t)1000) == pdTRUE)
        {
            ESP_LOGI("DIRECT_METHOD", "Bloquando acesso a memoria NVS");
            if (xSemaphoreTake(xSemaphoreAcessoInternet, (TickType_t)1000) == pdTRUE)
            {
                ESP_LOGI("DIRECT_METHOD", "Bloquando acesso a Internet por outras funcoes");

                cJSON *root_object = cJSON_Parse((char *)payload);

                char *uri_cert = cJSON_GetObjectItem(root_object, "uriCertificado")->valuestring;
                char *uri_chave = cJSON_GetObjectItem(root_object, "uriChave")->valuestring;
                char *new_common_name = cJSON_GetObjectItem(root_object, "commonName")->valuestring;

                ESP_LOGI("Download", "BAIXANDO CERTIFICADO");

                char *buffer_cert = baixar_arquivo(uri_cert);
                
                esp_orion_reset_status = 1;

                if (buffer_cert != NULL)
                {
                    FILE *novo_arquivo_cert = abrir_arquivo_e_montar_particao("/spiffs/leaf_certificate.pem", "storage_certs", "w+");

                    fprintf(novo_arquivo_cert, buffer_cert);

                    fechar_arquivo("storage_certs", novo_arquivo_cert);
                }
                else
                {
                    ESP_LOGE("DIRECT_METHOD", "ERRO NA ESCRITA DOS CERTIFICADOS");
                    esp_orion_reset_status = 0;
                }
                
                free(buffer_cert);                
                ESP_LOGI("Download", "BAIXANDO CHAVE");
                char *buffer_key = baixar_arquivo(uri_chave);

                if (buffer_key != NULL)
                {    
                    FILE *novo_arquivo_chave = abrir_arquivo_e_montar_particao("/spiffs/leaf_private_key.pem", "storage_certs", "w+");

                    fprintf(novo_arquivo_chave, buffer_key);

                    fechar_arquivo("storage_certs", novo_arquivo_chave);
                }
                else
                {
                    ESP_LOGE("DIRECT_METHOD", "ERRO NA ESCRITA DOS CERTIFICADOS");
                    esp_orion_reset_status = 0;
                }

                free(buffer_key);
                
                cJSON_Delete(root_object);

                ESP_LOGI("Download", "ESCREVENDO ARQUIVO DE REDE");

                FILE *arquivo_rede = abrir_arquivo_e_montar_particao("/spiffs/rede.txt", "storage_rede", "r");

                char string_rede[TAMANHO_STRING_REDE];
                char config_rede[5];
                char SSID[TAMANHO_MENSAGEM];
                char senha[TAMANHO_MENSAGEM];
                char campo_extra_gsm[TAMANHO_MENSAGEM];

                fgets(string_rede, (sizeof(string_rede)), arquivo_rede);
                fgets(string_rede, (sizeof(string_rede)), arquivo_rede);

                fechar_arquivo("storage_rede", arquivo_rede);

                sprintf(config_rede, parse_field(string_rede, 1, char_separator));
                sprintf(SSID, parse_field(string_rede, 2, char_separator));
                sprintf(senha, parse_field(string_rede, 3, char_separator));

                arquivo_rede = abrir_arquivo_e_montar_particao("/spiffs/rede.txt", "storage_rede", "w");

                if (config_rede[0] == '3')
                {
                    sprintf(campo_extra_gsm, parse_field(string_rede, 4, char_separator));

                    sprintf(string_rede, "%s*%s*%s*%s*%s*", 
                        config_rede, 
                        SSID, 
                        senha,
                        campo_extra_gsm,
                        new_common_name
                    );
                }
                else
                {
                    sprintf(string_rede, "%s*%s*%s*%s*", 
                        config_rede, 
                        SSID, 
                        senha,
                        new_common_name
                    );
                }
                
                fprintf(arquivo_rede, string_rede);

                fechar_arquivo("storage_rede", arquivo_rede);

                ESP_LOGI("Download", "DOWNLOAD FINALIZADO - REINICIANDO GATEWAY");

                xSemaphoreGive(xSemaphoreAcessoInternet);
                xSemaphoreGive(xSemaphoreAcessoSpiffs);
            }
        }
    }
    else if(is_str_equal(FLAG_TROCA, (char*) method_name))
    {
        usar_configs_fabrica = !usar_configs_fabrica;
        mudar_campo_usar_config_fabr(usar_configs_fabrica);
    }
    else
    {
        // All other entries are ignored.
        const char deviceMethodResponse[] = "{ }";
        *response_size = sizeof(deviceMethodResponse) - 1;
        *response = malloc(*response_size);
        if (*response != NULL)
        {
            (void)memcpy(*response, deviceMethodResponse, *response_size);
        }
        result = -1;
    }
    return result;
}

void deviceTwinCallback(DEVICE_TWIN_UPDATE_STATE update_state, const unsigned char *payLoad, size_t size, void *userContextCallback)
{
    (void)update_state;
    (void)size;
    Device *oldDevice = (Device *)userContextCallback;
    Device *newDevice = parseFromJson((const char *)payLoad, update_state);

    if (newDevice != NULL)
    {
        if (xSemaphoreTake(xSemaphoreDeviceTwin, (TickType_t)100) == pdTRUE)
        {
            /* BLOQUEIA O ACESSO A deviceTwinTempoExecucao para reescrever suas caracteristicas */

            //Se OTA é via método direto, acredito que aqui deve ser algo read-only.
            if (newDevice->versao_firmware != NULL)
            {
                if ((oldDevice->versao_firmware != NULL) && (strcmp(oldDevice->versao_firmware, newDevice->versao_firmware) != 0))
                {
                    free(oldDevice->versao_firmware);
                    oldDevice->versao_firmware = NULL;
                }

                if (oldDevice->versao_firmware == NULL)
                {
                    ESP_LOGI("DEVICE_TWIN", "Received a new url versao firmware = %s", newDevice->versao_firmware);
                    if (NULL != (oldDevice->versao_firmware = malloc(strlen(newDevice->versao_firmware) + 1)))
                    {
                        (void)strcpy(oldDevice->versao_firmware, newDevice->versao_firmware);
                        free(newDevice->versao_firmware);
                    }
                }
            }

            if (newDevice->net_id_gateway != NULL)
            {
                if ((oldDevice->net_id_gateway != NULL) && (strcmp(oldDevice->net_id_gateway, newDevice->net_id_gateway) != 0))
                {
                    free(oldDevice->net_id_gateway);
                    oldDevice->net_id_gateway = NULL;
                }

                if (oldDevice->net_id_gateway == NULL)
                {
                    printf("Received a new net_id_gateway = %s\n", newDevice->net_id_gateway);
                    if (NULL != (oldDevice->net_id_gateway = malloc(strlen(newDevice->net_id_gateway) + 1)))
                    {
                        (void)strcpy(oldDevice->net_id_gateway, newDevice->net_id_gateway);
                        free(newDevice->net_id_gateway);
                    }
                }
            }

            if (newDevice->ble_address_gateway != NULL)
            {
                if ((oldDevice->ble_address_gateway != NULL) && (!is_str_equal(oldDevice->ble_address_gateway, newDevice->ble_address_gateway)) )
                {
                    free(oldDevice->ble_address_gateway);
                    oldDevice->ble_address_gateway = NULL;
                }

                if (oldDevice->ble_address_gateway == NULL)
                {
                    printf("Received a new ble_address_gateway = %s\n", newDevice->ble_address_gateway);
                    if (NULL != (oldDevice->ble_address_gateway = malloc(strlen(newDevice->ble_address_gateway) + 1)))
                    {
                        (void)strcpy(oldDevice->ble_address_gateway, newDevice->ble_address_gateway);
                        free(newDevice->ble_address_gateway);
                    }
                }
            }

            if (newDevice->tipo_conexao != NULL)
            {
                if ((oldDevice->tipo_conexao != NULL) && (strcmp(oldDevice->tipo_conexao, newDevice->tipo_conexao) != 0))
                {
                    free(oldDevice->tipo_conexao);
                    oldDevice->tipo_conexao = NULL;
                }

                if (oldDevice->tipo_conexao == NULL)
                {
                    ESP_LOGI("DEVICE_TWIN", "Received a new tipo conexao = %s", newDevice->tipo_conexao);
                    if (NULL != (oldDevice->tipo_conexao = malloc(strlen(newDevice->tipo_conexao) + 1)))
                    {
                        (void)strcpy(oldDevice->tipo_conexao, newDevice->tipo_conexao);
                        free(newDevice->tipo_conexao);
                    }
                }
                // tratar caso tenha mudanca do tipo de conexao
                
                if ( tipo_conexao_ativa == WIFI && strcmp(oldDevice->tipo_conexao, "wifi") != 0 ) {
                    // esta com a conexao wifi e no hub esta conexao ethernet
                    // flag_trocar_tipo_conexao indica a troca a ser efetuada na conexão:
                        /*  0 - Não haverá troca;
                            1 - Troca de wifi para ethernet;
                            2 - Troca de Ethernet para wifi;
                            3 - Troca de PPPoS para wifi;
                            4 - Troca de Wifi para PPPoS;
                        */
                    if ( strcmp(oldDevice->tipo_conexao, "ethernet") == 0 ) {
                        flag_trocar_tipo_conexao = TROCA_WIFI_ETH;
                    }
                    else if ( strcmp(oldDevice->tipo_conexao, "pppos") == 0 ) {
                        flag_trocar_tipo_conexao = TROCA_WIFI_PPPOS;
                    }
                }

                if ( tipo_conexao_ativa == ETHERNET && strcmp(oldDevice->tipo_conexao, "ethernet") != 0 ) {
                   if (strcmp(oldDevice->tipo_conexao, "wifi") == 0 ) {
                        flag_trocar_tipo_conexao = TROCA_ETH_WIFI;
                    }
                }

                if ( tipo_conexao_ativa == PPPOS && strcmp(oldDevice->tipo_conexao, "pppos") != 0 ) {
                    if ( strcmp(oldDevice->tipo_conexao, "wifi") == 0) {
                        flag_trocar_tipo_conexao = TROCA_PPPOS_WIFI;
                    }
                }
            }

            if (newDevice->chave_update_firmware != NULL)
            {
                if ((oldDevice->chave_update_firmware != NULL) && (strcmp(oldDevice->chave_update_firmware, newDevice->chave_update_firmware) != 0))
                {
                    free(oldDevice->chave_update_firmware);
                    oldDevice->chave_update_firmware = NULL;
                }

                if (oldDevice->chave_update_firmware == NULL)
                {
                    ESP_LOGI("DEVICE_TWIN", "Received a new chave_update_firmware = %s", newDevice->chave_update_firmware);
                    if (NULL != (oldDevice->chave_update_firmware = malloc(strlen(newDevice->chave_update_firmware) + 1)))
                    {
                        (void)strcpy(oldDevice->chave_update_firmware, newDevice->chave_update_firmware);
                        free(newDevice->chave_update_firmware);
                    }
                }
            }

            if (newDevice->data_update_firmware != NULL)
            {
                if ((oldDevice->data_update_firmware != NULL) && (strcmp(oldDevice->data_update_firmware, newDevice->data_update_firmware) != 0))
                {
                    free(oldDevice->data_update_firmware);
                    oldDevice->data_update_firmware = NULL;
                }

                if (oldDevice->data_update_firmware == NULL)
                {
                    ESP_LOGI("DEVICE_TWIN", "Received a new data_update_firmware = %s", newDevice->data_update_firmware);
                    if (NULL != (oldDevice->data_update_firmware = malloc(strlen(newDevice->data_update_firmware) + 1)))
                    {
                        (void)strcpy(oldDevice->data_update_firmware, newDevice->data_update_firmware);
                        free(newDevice->data_update_firmware);
                    }
                }
            }

            if (newDevice->config_rede.ip != NULL)
            {
                if ((oldDevice->config_rede.ip != NULL) && (strcmp(oldDevice->config_rede.ip, newDevice->config_rede.ip) != 0))
                {
                    free(oldDevice->config_rede.ip);
                    oldDevice->config_rede.ip = NULL;
                }

                if (oldDevice->config_rede.ip == NULL)
                {
                    ESP_LOGI("DEVICE_TWIN", "Received a new ip = %s", newDevice->config_rede.ip);
                    if (NULL != (oldDevice->config_rede.ip = malloc(strlen(newDevice->config_rede.ip) + 1)))
                    {
                        (void)strcpy(oldDevice->config_rede.ip, newDevice->config_rede.ip);
                        free(newDevice->config_rede.ip);
                    }
                }
            }

            if (newDevice->config_rede.DNS != NULL)
            {
                if ((oldDevice->config_rede.DNS != NULL) && (strcmp(oldDevice->config_rede.DNS, newDevice->config_rede.DNS) != 0))
                {
                    free(oldDevice->config_rede.DNS);
                    oldDevice->config_rede.DNS = NULL;
                }

                if (oldDevice->config_rede.DNS == NULL)
                {
                    ESP_LOGI("DEVICE_TWIN", "Received a new DNS = %s", newDevice->config_rede.DNS);
                    if (NULL != (oldDevice->config_rede.DNS = malloc(strlen(newDevice->config_rede.DNS) + 1)))
                    {
                        (void)strcpy(oldDevice->config_rede.DNS, newDevice->config_rede.DNS);
                        free(newDevice->config_rede.DNS);
                    }
                }
            }

            if (newDevice->config_rede.ativo != NULL)
            {
                if ((oldDevice->config_rede.ativo != NULL) && (strcmp(oldDevice->config_rede.ativo, newDevice->config_rede.ativo) != 0))
                {
                    free(oldDevice->config_rede.ativo);
                    oldDevice->config_rede.ativo = NULL;
                }

                if (oldDevice->config_rede.ativo == NULL)
                {
                    ESP_LOGI("DEVICE_TWIN", "Received a new ativo = %s", newDevice->config_rede.ativo);
                    if (NULL != (oldDevice->config_rede.ativo = malloc(strlen(newDevice->config_rede.ativo) + 1)))
                    {
                        (void)strcpy(oldDevice->config_rede.ativo, newDevice->config_rede.ativo);
                        free(newDevice->config_rede.ativo);
                    }
                }

                if ( strcmp(oldDevice->config_rede.ativo, "true") == 0 ) {
                    flag_verificar_troca_conexao = 1;
                }
                else
                {
                    flag_verificar_troca_conexao = 0;
                }
                    
            }

            if (newDevice->config_rede.gateway != NULL)
            {
                if ((oldDevice->config_rede.gateway != NULL) && (strcmp(oldDevice->config_rede.gateway, newDevice->config_rede.gateway) != 0))
                {
                    free(oldDevice->config_rede.gateway);
                    oldDevice->config_rede.gateway = NULL;
                }

                if (oldDevice->config_rede.gateway == NULL)
                {
                    ESP_LOGI("DEVICE_TWIN", "Received a new gateway = %s", newDevice->config_rede.gateway);
                    if (NULL != (oldDevice->config_rede.gateway = malloc(strlen(newDevice->config_rede.gateway) + 1)))
                    {
                        (void)strcpy(oldDevice->config_rede.gateway, newDevice->config_rede.gateway);
                        free(newDevice->config_rede.gateway);
                    }
                }
            }

            if (newDevice->config_rede.usuario_gsm != NULL)
            {
                if ((oldDevice->config_rede.usuario_gsm != NULL) && (strcmp(oldDevice->config_rede.usuario_gsm, newDevice->config_rede.usuario_gsm) != 0))
                {
                    free(oldDevice->config_rede.usuario_gsm);
                    oldDevice->config_rede.usuario_gsm = NULL;
                }

                if (oldDevice->config_rede.usuario_gsm == NULL)
                {
                    printf("Received a new usuario_gsm = %s\n", newDevice->config_rede.usuario_gsm);
                    if (NULL != (oldDevice->config_rede.usuario_gsm = malloc(strlen(newDevice->config_rede.usuario_gsm) + 1)))
                    {
                        (void)strcpy(oldDevice->config_rede.usuario_gsm, newDevice->config_rede.usuario_gsm);
                        free(newDevice->config_rede.usuario_gsm);
                    }
                }
            }

            if (newDevice->config_rede.netmask != NULL)
            {
                if ((oldDevice->config_rede.netmask != NULL) && (strcmp(oldDevice->config_rede.netmask, newDevice->config_rede.netmask) != 0))
                {
                    free(oldDevice->config_rede.netmask);
                    oldDevice->config_rede.netmask = NULL;
                }

                if (oldDevice->config_rede.netmask == NULL)
                {
                    ESP_LOGI("DEVICE_TWIN", "Received a new netmask = %s", newDevice->config_rede.netmask);
                    if (NULL != (oldDevice->config_rede.netmask = malloc(strlen(newDevice->config_rede.netmask) + 1)))
                    {
                        (void)strcpy(oldDevice->config_rede.netmask, newDevice->config_rede.netmask);
                        free(newDevice->config_rede.netmask);
                    }
                }
            }

            if (newDevice->config_rede.ssid != NULL)
            {
                if ((oldDevice->config_rede.ssid != NULL) && (strcmp(oldDevice->config_rede.ssid, newDevice->config_rede.ssid) != 0))
                {
                    free(oldDevice->config_rede.ssid);
                    oldDevice->config_rede.ssid = NULL;
                }

                if (oldDevice->config_rede.ssid == NULL)
                {
                    ESP_LOGI("DEVICE_TWIN", "Received a new ssid = %s", newDevice->config_rede.ssid);
                    if (NULL != (oldDevice->config_rede.ssid = malloc(strlen(newDevice->config_rede.ssid) + 1)))
                    {
                        (void)strcpy(oldDevice->config_rede.ssid, newDevice->config_rede.ssid);
                        free(newDevice->config_rede.ssid);
                    }
                }
            }

            if (newDevice->config_rede.senha != NULL)
            {
                if ((oldDevice->config_rede.senha != NULL) && (strcmp(oldDevice->config_rede.senha, newDevice->config_rede.senha) != 0))
                {
                    free(oldDevice->config_rede.senha);
                    oldDevice->config_rede.senha = NULL;
                }

                if (oldDevice->config_rede.senha == NULL)
                {
                    ESP_LOGI("DEVICE_TWIN", "Received a new senha = %s", newDevice->config_rede.senha);
                    if (NULL != (oldDevice->config_rede.senha = malloc(strlen(newDevice->config_rede.senha) + 1)))
                    {
                        (void)strcpy(oldDevice->config_rede.senha, newDevice->config_rede.senha);
                        free(newDevice->config_rede.senha);
                    }
                }
            }

            if (newDevice->num_sensores != 0)
            {
                /* Aqui pode ser alterada a variavel global para o timer de envio de mensagens ao hub */
                if (newDevice->num_sensores != oldDevice->num_sensores)
                {
                    ESP_LOGI("DEVICE_TWIN", "Received a new desired_num_sensores = %d", newDevice->num_sensores);
                    oldDevice->num_sensores = newDevice->num_sensores;
                }
            }
            else if (newDevice->num_sensores == 0)
            {
                vTaskDelay(750 / portTICK_PERIOD_MS);
                esp_restart();
            }

            if (newDevice->baud_rate_gateway != 0)
            {
                /* Aqui pode ser alterada a variavel global para o timer de envio de mensagens ao hub */
                if (newDevice->baud_rate_gateway != oldDevice->baud_rate_gateway)
                {
                    printf("Received a new desired baud rate = %d\n", newDevice->baud_rate_gateway);
                    oldDevice->baud_rate_gateway = newDevice->baud_rate_gateway;
                }
            }

            int contador;
            if (oldDevice->sensor != NULL)
            {
                free(oldDevice->sensor);
                oldDevice->sensor = NULL;
            }
            
            if(historico.historico_mensagens == NULL)
            {
                historico.historico_mensagens = malloc(sizeof(hist_mens)*newDevice->num_sensores);
            }
            else
            {
                historico.historico_mensagens = realloc(historico.historico_mensagens, sizeof(hist_mens)*newDevice->num_sensores);
            }

            oldDevice->sensor = (SensorState *)malloc(sizeof(SensorState)*newDevice->num_sensores );
            for (contador = 0; contador < newDevice->num_sensores; contador++)
            {
                //TODO: Implementar o uso de pullup/pulldown
                if (newDevice->sensor[contador].digitalTwinId != NULL)
                {
                    if ((oldDevice->sensor[contador].digitalTwinId != NULL))
                    {
                        oldDevice->sensor[contador].digitalTwinId = NULL;
                    }

                    if (oldDevice->sensor[contador].digitalTwinId == NULL)
                    {
                        ESP_LOGI("DEVICE_TWIN", "Received a new sensor[%d]_digitalTwinId = %s", contador, newDevice->sensor[contador].digitalTwinId);
                        if (NULL != (oldDevice->sensor[contador].digitalTwinId = malloc(strlen(newDevice->sensor[contador].digitalTwinId) + 1)))
                        {
                            (void)strcpy(oldDevice->sensor[contador].digitalTwinId, newDevice->sensor[contador].digitalTwinId);
                            free(newDevice->sensor[contador].digitalTwinId);
                        }
                    }
                }
                //TODO: implementar a habilitação dos pinos??
                if (newDevice->sensor[contador].bleMeshAddr != 0)
                {
                    /* Id de cada sensor */
                    if (newDevice->sensor[contador].bleMeshAddr != oldDevice->sensor[contador].bleMeshAddr)
                    {
                        ESP_LOGI("DEVICE_TWIN", "Received a new sensor[%d].bleMeshAddr = %d", contador, newDevice->sensor[contador].bleMeshAddr);
                        oldDevice->sensor[contador].bleMeshAddr = newDevice->sensor[contador].bleMeshAddr;
                    }
                }

                historico.historico_mensagens[contador].old_saidas = 0;
                historico.historico_mensagens[contador].old_entradas = 0;
                historico.historico_mensagens[contador].obstrucao = 0;
                historico.historico_mensagens[contador].msgs_perdidas = 0;
                historico.historico_mensagens[contador].msgs_repetidas = 0;
                historico.historico_mensagens[contador].erros_crc = 0;
                historico.historico_mensagens[contador].reinicios = 0;
                historico.historico_mensagens[contador].heart_beat = 0;

            }                                     // end for escrita de sensors
            xSemaphoreGive(xSemaphoreDeviceTwin); // libera acesso a variavel
        }                                         // end if semaforo
        free(newDevice->sensor);
        free(newDevice);
        // Reporta para o hub
        char *reportedProperties = serializeToJson(oldDevice);
        (void)IoTHubDeviceClient_LL_SendReportedState(iotHubClientHandle, (const unsigned char *)reportedProperties, strlen(reportedProperties), reportedStateCallback, NULL);
        free(reportedProperties);

        //executa a mudança de conectividade se houver
        // troca_parametros_rede();

        escreve_configuracao_rede_em_memoria();
        leitura_nvs_historico();
    } // end device != null
    else
    {
        printf("Error: JSON parsing failed!\r\n");
    }
}

void reportedStateCallback(int status_code, void *userContextCallback)
{
    (void)userContextCallback;
    uint32_t heaps;
    heaps = esp_get_free_heap_size();
    printf("Free heap IotHub_Mensagens: %d\n", heaps);
    printf("Device Twin reported properties update completed with result: %d\r\n", status_code);

    if (status_code >= 200 || status_code < 300)
        status_update_device_twin = 1; // sinaliza que os dados do device twin foram recebidos com sucesso
}

void InicializarIothubDeviceTwin(uint8_t verificadorIothub, char * dps_h, char * dps_i)
{

    desativar_iothub = 0 ;
    flag_trocar_tipo_conexao = MANTEM_REDE;
    esp_orion_reset_status = 0;
    // Select the Protocol to use with the connection

    if (verificadorIothub != 0)
    {
        (void)printf("Failed to initialize the Device Twin platform.\r\n");
    }
    else
    {
        if ((iotHubClientHandle = IoTHubDeviceClient_LL_CreateFromDeviceAuth(dps_h, dps_i,  MQTT_Protocol) ) == NULL)
        //if ((iotHubClientHandle = IoTHubDeviceClient_LL_CreateFromConnectionString(connectionString, protocol)) == NULL)
        {
            (void)printf("ERROR: iotHubClientHandle is NULL!\r\n");
        }
        else
        {
            // Uncomment the following lines to enable verbose logging (e.g., for debugging).
            //bool traceOn = true;
            //(void)IoTHubDeviceClient_SetOption(iotHubClientHandle, OPTION_LOG_TRACE, &traceOn);

#ifdef SET_TRUSTED_CERT_IN_SAMPLES
            // For mbed add the certificate information
            if (IoTHubDeviceClient_LL_SetOption(iotHubClientHandle, "TrustedCerts", certificates) != IOTHUB_CLIENT_OK)
            {
                ESP_LOGE("DEVICE_TWIN", "failure to set option \"TrustedCerts\"");
            }
#endif // SET_TRUSTED_CERT_IN_SAMPLES


            char *reportedProperties = serializeToJson(&device_alocado);
            // tenta devolver o semaforo no caso de ter sido pego
            // pela funcao que realiza a troca do tipo de conexao
            xSemaphoreGive(xSemaphoreAcessoInternet);

            if (reportedProperties != NULL)
            {
                (void)IoTHubDeviceClient_LL_SetDeviceMethodCallback(iotHubClientHandle, deviceMethodCallback, NULL);
                (void)IoTHubDeviceClient_LL_SetDeviceTwinCallback(iotHubClientHandle, deviceTwinCallback, &device_alocado);
                
                //esse loop infinito não tem outra condição de saída?
                //há tratamento de desconexão aqui?
                //TODO: setar a global do desativar_iot_hub para 1 quando houver desconexão com a internet
                //testar se jhá conectividade antes de mandar a DoWork
                //para testar conectividade, pode-se setar o IoTHubClientCore_LL_SetConnectionStatusCallback, e este callback pode setar uma flag global de "desconectado"
                //jpá os eventos de reconexão de ehternet e wifi poderiam setar este flag para 1 novamente
                //o recomendado é cahamr a cada 100ms no máximo
                while (1)
                {
                    //todo: sério candidato a loop infinito e hangup eterno
                        IoTHubDeviceClient_LL_DoWork(iotHubClientHandle);
                        //ThreadAPI_Sleep(10);
                        vTaskDelay(50 / portTICK_PERIOD_MS);
                        if( desativar_iothub == 1) {
                            // seta a flag para quebrar o loop da função de device twin
                            vTaskDelay(1000/ portTICK_PERIOD_MS);
                            desativar_conexao_iothub();
                            break;
                        }
                        if( esp_orion_reset_status == 1) {
                            vTaskDelay(10000/ portTICK_PERIOD_MS);
                            esp_restart();
                        }
                }

                free(reportedProperties);
            }
            else
            {
                free(reportedProperties);
                printf("Error: JSON serialization failed!\r\n");
            }
            //IoTHubDeviceClient_LL_Destroy(iotHubClientHandle);
        }

        //IoTHub_Deinit();
    }
}

int verifica_status_device_twin()
{
    return status_update_device_twin;
}

void reporta_status_ota(char *ota_status)
{

    Device device; // vai representar o valor das propriedades
    memset(&device, 0, sizeof(Device));
    device.ota_update_status = ota_status;

    char *reportedProperties = serializeToJson(&device);
    if (reportedProperties != NULL)
    {
        (void)IoTHubDeviceClient_LL_SendReportedState(iotHubClientHandle, (const unsigned char *)reportedProperties, strlen(reportedProperties), reportedStateCallback, NULL);

        //Por que 10?? tem a ver com o numero de sensores?
        while (
            IoTHubClient_LL_GetSendStatus(iotHubClientHandle, &status) == IOTHUB_CLIENT_OK &&
            status == IOTHUB_CLIENT_SEND_STATUS_BUSY
        )
        {
            IoTHubDeviceClient_LL_DoWork(iotHubClientHandle);
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
        free(reportedProperties);
    }
    else
    {
        free(reportedProperties);
        printf("Error: JSON serialization failed!\r\n");
    }
}

void reporta_status_configs_rede(void)
{
    Device device; // vai representar o valor das propriedades
    memset(&device, 0, sizeof(Device));
    device.config_rede.ip = rede_ip;
    device.config_rede.netmask = rede_netmask;
    device.config_rede.gateway = rede_gateway;
    device.config_rede.DNS = rede_dns;

    char *reportedProperties = serializeToJson(&device);
    if (reportedProperties != NULL)
    {
        (void)IoTHubDeviceClient_LL_SendReportedState(iotHubClientHandle, (const unsigned char *)reportedProperties, strlen(reportedProperties), reportedStateCallback, NULL);

        //porque esse loop novamente? tem a ver com sensores?
        while (
            IoTHubClient_LL_GetSendStatus(iotHubClientHandle, &status) == IOTHUB_CLIENT_OK &&
            status == IOTHUB_CLIENT_SEND_STATUS_BUSY
        )
        {
            IoTHubDeviceClient_LL_DoWork(iotHubClientHandle);
            //ThreadAPI_Sleep(10);
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
        free(reportedProperties);
    }
    else
    {
        free(reportedProperties);
        printf("Error: JSON serialization failed!\r\n");
    }
}


void desativar_conexao_iothub(void) {
    // seta a flag para quebrar o loop da função de device twin
    ESP_LOGI("DEVICE_TWIN", "Desativando iothub");
    IoTHubDeviceClient_LL_Destroy(iotHubClientHandle);
    IoTHub_Deinit();
    // TROCA O TIPO DE CONEXAO
    // flag_trocar_tipo_conexao indica a troca a ser efetuada na conexão:
                        /*  0 - Não haverá troca;
                            1 - Troca de wifi para ethernet;
                            2 - Troca de Ethernet para wifi;
                            3 - Troca de PPPoS para wifi;
                            4 - Troca de Wifi para PPPoS;
                        */
    if(flag_trocar_tipo_conexao == TROCA_ETH_WIFI) {
        desativa_ethernet();
        tipo_conexao_ativa = WIFI;
    }
    else if( flag_trocar_tipo_conexao == TROCA_WIFI_ETH) {
        desliga_wifi();
        tipo_conexao_ativa = ETHERNET;
    }
    else if(flag_trocar_tipo_conexao == TROCA_PPPOS_WIFI){
        desliga_pppos();
        tipo_conexao_ativa = WIFI;
    }
    else if(flag_trocar_tipo_conexao == TROCA_WIFI_PPPOS){
        desliga_wifi();
        tipo_conexao_ativa = PPPOS;
    }
    else {
        if (tipo_conexao_ativa == WIFI) {
            desliga_wifi();
        }
        else {
            desativa_ethernet();
        }
    }
    ESP_LOGI("DEVICE_TWIN", "TIPO CONEXAO: %d", tipo_conexao_ativa);

    /* estava com wifi ativado ao chamar a troca de conexao */
    if( tipo_conexao_ativa == ETHERNET) {
        inicializa_ethernet(trocar_tipo_conexao);
    }
    if( tipo_conexao_ativa == WIFI) {
    // estava com ethernet ativado ao chamar a troca de conexao 
        wifi_init_sta(trocar_tipo_conexao);
    }

    if ( tipo_conexao_ativa == PPPOS) {
        inicializar_pppos();
    }
}

void trocar_conexao(int param) {
    // param = 1 -> Inicializa a nova rede com parametros estaticos
    // param = 0 -> Inicializa nova rede com parâmetros dinâmicos
    desativar_iothub = 1;
    // o tipo de conexao deve ser trocado
    trocar_tipo_conexao = param;
}

void troca_parametros_rede(void)
{
    /* Essa funcao eh chamada em toda atualizacao do device twin e, verifica se houve alguma mudanca
     * em relacao a rede que esta sendo utilizada pelo esp no momento
     */
    int status, flag_mudanca=0, flag_trocou_conexao=0;
    // pega o semaforo para bloquear a thread de envio de mensagens
    if (xSemaphoreTake(xSemaphoreAcessoInternet, (TickType_t)100) == pdTRUE)
    {
        ESP_LOGI("DEVICE_TWIN", "Pegou o semaforo!");
        // verifica se existe mudanca nas configs de wifi(ssid, senha)
        char *ssidNovo = deviceTwinTempoExecucao->config_rede.ssid;
        char *senhaNova = deviceTwinTempoExecucao->config_rede.senha;
        char *ssidAntigo = rede_wifi_ssid;
        char *senhaAntiga = rede_wifi_senha;
        if( tipo_conexao_ativa == 1 || flag_trocar_tipo_conexao == TROCA_ETH_WIFI ) {
            if( ssidNovo != NULL && strcmp(ssidNovo, ssidAntigo) != 0 ) {
                // teve mudanca no SSID
                ESP_LOGI("troca_conexao", "Mudou ssid!");
                rede_wifi_ssid = ssidNovo;
                flag_mudanca = 1;
                flag_trocar_tipo_conexao = TROCA_WIFI_CONFIG;
            }
            else
            {
                ESP_LOGE("Spiffs", "ssidNovo é null");
            }
            
            if( senhaNova != NULL && strcmp(senhaNova, senhaAntiga) != 0 ) {
                // teve mudanca no senha
                ESP_LOGI("troca_conexao", "Mudou senha");
                rede_wifi_senha = senhaNova;
                flag_mudanca = 1;
                flag_trocar_tipo_conexao = TROCA_WIFI_CONFIG;
            }
            else
            {
                ESP_LOGE("Spiffs", "senhaNova é null");
            }
            if( flag_verificar_troca_conexao == 0 && flag_mudanca == 1 && flag_trocar_tipo_conexao != 1){
                
                trocar_conexao(0);
                flag_trocou_conexao = 1;
            }          
        }

        status = verifica_parametros_rede(
            deviceTwinTempoExecucao->config_rede.ip, 
            deviceTwinTempoExecucao->config_rede.netmask, 
            deviceTwinTempoExecucao->config_rede.gateway, 
            deviceTwinTempoExecucao->config_rede.DNS
        );

        ESP_LOGI("troca_conexao", "VALOR DO STATUS: %d", status);

        if ( status != 0 && flag_verificar_troca_conexao == TROCA_WIFI_ETH ) {
            // deve trocar a rede, config estatica ativa
            rede_ip = deviceTwinTempoExecucao->config_rede.ip;
            rede_gateway = deviceTwinTempoExecucao->config_rede.gateway;
            rede_netmask = deviceTwinTempoExecucao->config_rede.netmask;
            rede_dns = deviceTwinTempoExecucao->config_rede.DNS;
            
            trocar_conexao(1);
            flag_trocou_conexao = 1;
        }
        else if(flag_verificar_troca_conexao == MANTEM_REDE && status == 0) {
            // caso esteja operando em configuracao estatica e quer trocar para config dinamica
            
            trocar_conexao(0);
            flag_trocou_conexao = 1;
        }
        else if(flag_trocar_tipo_conexao != MANTEM_REDE) {
            // verifica se mudou o tipo de rede (wifi/ethernet)
            
            trocar_conexao(0);
            flag_trocou_conexao = 1;
        }
        // caso chegue ao fim sem trocar de conexao devolve o semaforo
        if( flag_trocou_conexao != 1) {
            ESP_LOGI("DEVICE_TWIN", "Nao teve troca de conexao, devolvendo o semaforo!");
            xSemaphoreGive(xSemaphoreAcessoInternet);
        }

    }

    
}

int verifica_parametros_rede(char * ip, char * netmask, char * gateway, char * dns)
{
    // caso os parametros sejam iguais aos que estao sendo usados atualmente retorna 0
    int soma = 0;
    // verificar se as strings contendo as configuracoes da rede nao sao NULL
    if( rede_dns != NULL && rede_ip != NULL && rede_netmask != NULL && rede_gateway != NULL) {
        if (ip != NULL) {
            soma += abs(strcmp(rede_ip, ip));
        }
        else
        {
            ESP_LOGE("Spiffs", "ip é nulo");
        }
        if (netmask != NULL) {
            soma += abs(strcmp(rede_netmask, netmask));
        }
        else
        {
            ESP_LOGE("Spiffs", "netmask é nulo");
        }
        if (gateway != NULL) {
            soma += abs(strcmp(rede_gateway, gateway));
        }
        else
        {
            ESP_LOGE("Spiffs", "gateway é nulo");
        }
        if (dns != NULL) {
            soma += abs(strcmp(rede_dns, dns));
        }
        else
        {
            ESP_LOGE("Spiffs", "dns é nulo");
        }
        
    }
    else {
        ESP_LOGI("Spiffs", "String de configuracao de rede era NULL");
    }
    return soma;
}

int escreve_configuracao_rede_em_memoria(void)
{
    int tipo_conexao, estatica_ativo;
    char string_escrita[255], string_buffer[255];
    string_escrita[0] = '\0';
    // verifica qual o tipo de conexao que deve estar ativa
    if ( deviceTwinTempoExecucao->tipo_conexao == NULL ) {
        ESP_LOGE("Spiffs", "Tipo de conexao nao existente");
        tipo_conexao = 4;
    }
    else if( is_str_equal(deviceTwinTempoExecucao->tipo_conexao, "wifi") )
        tipo_conexao = WIFI;
    else if ( is_str_equal(deviceTwinTempoExecucao->tipo_conexao, "ethernet") )
        tipo_conexao = ETHERNET;
    else if ( is_str_equal(deviceTwinTempoExecucao->tipo_conexao, "pppos") )
        tipo_conexao = PPPOS;
    else
    {
        ESP_LOGE("Spiffs", "Tipo de conexao nao existente");
        return -1;
    }

    // verifica se deve usar parametros estaticos ou dinamicos
    if (deviceTwinTempoExecucao->config_rede.ativo == NULL)
        estatica_ativo = 0;
    else if( (is_str_true(deviceTwinTempoExecucao->config_rede.ativo)) && tipo_conexao != 3 )
        estatica_ativo = 1;
    else
        estatica_ativo = 0;

    // caso seja uma conexao valida, inicializa a particao de configs de rede
        
    sprintf(string_buffer, "\n%d%d%s", tipo_conexao, estatica_ativo, char_separator);
    strcat(string_escrita, string_buffer);

    // se estiver usando wifi
    switch (tipo_conexao)
    {
        case WIFI:
        {
            sprintf(string_buffer, "%s%s%s%s", deviceTwinTempoExecucao->config_rede.ssid, char_separator, deviceTwinTempoExecucao->config_rede.senha, char_separator);
        }
        break;

        case PPPOS:
        {
            sprintf(string_buffer, "%s%s%s%s%s%s", deviceTwinTempoExecucao->config_rede.ssid, char_separator, deviceTwinTempoExecucao->config_rede.senha, char_separator, deviceTwinTempoExecucao->config_rede.usuario_gsm, char_separator);
        }
        break;
    }
    strcat(string_escrita, string_buffer);
    

    sprintf(string_buffer, "%s%s", cert_common_name, char_separator);
    strcat(string_escrita, string_buffer);

    // caso o parametro de config estatica seja verdadeiro
    if(estatica_ativo) {
        sprintf(string_buffer, "/%s/%s/%s/%s/", 
            deviceTwinTempoExecucao->config_rede.ip, 
            deviceTwinTempoExecucao->config_rede.gateway,
            deviceTwinTempoExecucao->config_rede.netmask, 
            deviceTwinTempoExecucao->config_rede.DNS
        );
        strcat(string_escrita, string_buffer);
    }

    sprintf(string_buffer,"%s%s", id_scope, char_separator);
    strcat(string_escrita, string_buffer);

    sprintf(string_buffer,"%c%s", usar_configs_fabrica ? '1':'0', char_separator);
    strcat(string_escrita, string_buffer);

    printf("nova string e: %s\n", string_escrita);

    // Escreve novos dados na segunda linha
    memset(string_buffer, 0, 255);

    FILE* f = abrir_arquivo_e_montar_particao("/spiffs/rede.txt", "storage_rede", "r+");

    fgets(string_buffer, 255, f);

    if(strchr(string_buffer, '\n') != NULL)
        fseek(f, strlen(string_buffer)-1, SEEK_SET);

    fprintf(f, string_escrita);

    fechar_arquivo("storage_rede", f);

    // if(flag_trocar_tipo_conexao == TROCA_WIFI_CONFIG)
    // {
    //     esp_restart();
    // }

    return 0;
}