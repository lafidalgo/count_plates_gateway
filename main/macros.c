#include "macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BUFFER_CERT 20480
char *arquivo_buffer;
int len;

int is_str_equal(char *str1, char *str2)
{
    if (NULL == str1 && NULL == str2)
    {
        ESP_LOGE("MACROS", "AMBAS AS STRINGS SAO NULAS");
        return 0;
    }
    else if (NULL == str1)
    {
        ESP_LOGE("MACROS", "A PRIMEIRA STRING É NULA E O VALOR DA SEGUNDA É %s\n", str2);
        return 0;
    }
    else if (NULL == str2)
    {
        ESP_LOGE("MACROS", "A SEGUNDA STRING É NULA E O VALOR DA PRIMEIRA É %s\n", str1);
        return 0;
    }
    else if (0 == strcmp(str1, str2))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

int is_str_true(char *str)
{
    if (NULL == str)
    {
        ESP_LOGE("MACROS", "A STRING É NULA");
        return 0;
    }
    else if (0 == strcmp(str, "true") || 0 == strcmp(str, "True"))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

int is_str_false(char *str)
{
    if (NULL == str)
    {
        ESP_LOGI("MACROS", "A STRING É NULA");
        return 0;
    }
    else if (0 == strcmp(str, "false") || 0 == strcmp(str, "False"))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

int is_str_bool(char *str)
{
    if (NULL == str)
    {
        ESP_LOGI("MACROS", "A STRING É NULA");
        return 0;
    }
    else if (is_str_false(str) || is_str_true(str))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

int are_float_equal(float flt1, float flt2)
{
    float epsilon = 0.001;
    float faixa_sup = flt1 + (flt1 * epsilon);
    float faixa_inf = flt1 - (flt1 * epsilon);

    if (flt2 >= faixa_inf && flt2 <= faixa_sup)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
/*
char[8] get_string_address(int *address)
{
    char[8] address_string;

    sprint(address_string, "%02X%02X%02X%02X",
        (int)address[0],
        (int)address[1],
        (int)address[2],
        (int)address[3]
    );

    printf(address_string);

    return address_string;
}
*/

uint16_t get_bytes_address(char *address)
{
    uint16_t address_bytes;

    address_bytes = (int)strtol(address, NULL, 16);

    return address_bytes;
}

char *hex_to_string(char *hex)
{
    int *num = (int *)strtol(hex, NULL, 16);
    char *string = "";

    sprintf(string, "%n", num);

    return string;
}

/*int achar_indice_sensor_por_endereco(uint16_t bleMeshAddr)
{
    for (int j = 0; j < deviceTwinTempoExecucao->num_sensores; j++)
    {
        if (bleMeshAddr == deviceTwinTempoExecucao->sensor[j].bleMeshAddr)
        {
            return j;
        }
    }
    return -1;
}*/

FILE *abrir_arquivo_e_montar_particao(char *path, char *partition, char *type)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = partition,
        .max_files = 7,
        .format_if_mount_failed = true};

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE("Spiffs", "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE("Spiffs", "Failed to find SPIFFS partition");
        }
        else
        {
            ESP_LOGE("Spiffs", "Failed to initialize SPIFFS %s (%s)", partition, esp_err_to_name(ret));
        }
        return NULL;
    }

    FILE *arquivo = fopen(path, type);

    return arquivo;
}

int montar_particao(char *partition)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = partition,
        .max_files = 7,
        .format_if_mount_failed = true};

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE("Spiffs", "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE("Spiffs", "Failed to find SPIFFS partition");
        }
        else
        {
            ESP_LOGE("Spiffs", "Failed to initialize SPIFFS %s (%s)", partition, esp_err_to_name(ret));
        }
        return 0;
    }

    return 1;
}

int escrever_arquivo(char *string, FILE *arquivo)
{
    int confirmacao;

    if (arquivo != NULL)
    {
        confirmacao = fprintf(arquivo, string);

        if (confirmacao)
        {
            return 1;
        }
        else
        {
            ESP_LOGI("MACROS", "Erro ao escrever arquivo");
            return 0;
        }
    }
    else
    {
        ESP_LOGI("MACROS", "Erro ao abrir arquivo para escrita");
        return 0;
    }
}

char *parse_field(char *string, int campo, const char *sinalizador)
{
    char string_to_parse[120];
    sprintf(string_to_parse, "%s", string);

    char *result_string = strtok(string_to_parse, sinalizador);

    while (result_string != NULL)
    {
        if (!--campo)
        {
            return result_string;
        }

        result_string = strtok(NULL, sinalizador);
    }
    return NULL;
}

int string_to_int(char *string)
{
    char *ptr;

    int result = strtol(string, &ptr, 10);

    return result;
}

int fechar_arquivo(char *partition, FILE *arquivo)
{
    if (partition != NULL && arquivo != NULL)
    {
        fclose(arquivo);
        esp_vfs_spiffs_unregister(partition);

        return 1;
    }
    else
    {
        fclose(arquivo);
        esp_vfs_spiffs_unregister(partition);

        return 0;
    }
}

char *pegar_string_tempo_agora(void)
{
    time_t now;
    struct tm timeinfo;
    char strftime_buf[20], strfdate_buf[20];
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%T", &timeinfo);
    strftime(strfdate_buf, sizeof(strfdate_buf), "%Y-%m-%d", &timeinfo);
    char *stringAgora = (char *)malloc(sizeof(char) * 50);
    sprintf(stringAgora, "%sT%s.000Z", strfdate_buf, strftime_buf);

    return stringAgora;
}

int criar_arquivo_backup(char *particao, char *arquivo)
{
    char cabecalho[85] = "id,tempo_envio,batidas_a_b,batidas_b_a,nivel_bateria,obstrucao,digital_twin_id\n";
    FILE *arquivo_backup = abrir_arquivo_e_montar_particao(arquivo, particao, "w");
    int confirmacao = escrever_arquivo(cabecalho, arquivo_backup);
    fechar_arquivo(particao, arquivo_backup);

    return confirmacao;
}

reset_reasons pegar_motivo_crash(reset_reasons crash_reasons)
{
    esp_reset_reason_t reason = esp_reset_reason();

    if (
        reason == ESP_RST_SW ||
        reason == ESP_RST_EXT ||
        reason == ESP_RST_SDIO)
    {
        crash_reasons.software++;
    }
    else if (
        reason == ESP_RST_POWERON ||
        reason == ESP_RST_DEEPSLEEP)
    {
        crash_reasons.inicioNormal++;
    }
    else if (
        reason == ESP_RST_PANIC ||
        reason == ESP_RST_UNKNOWN ||
        reason == ESP_RST_BROWNOUT)
    {
        crash_reasons.crash++;
    }
    else
    {
        crash_reasons.watchdog++;
    }

    return crash_reasons;
}

esp_err_t _http_event_handle_download(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
    {
        ESP_LOGI("Download", "HTTP_EVENT_ERROR");
        return ESP_FAIL;
    }
    break;

    case HTTP_EVENT_ON_CONNECTED:
    {
        // ESP_LOGI("Download", "HTTP_EVENT_ON_CONNECTED");
    }
    break;

    case HTTP_EVENT_HEADER_SENT:
    {
        // ESP_LOGI("Download", "HTTP_EVENT_HEADER_SENT");
    }
    break;

    case HTTP_EVENT_ON_HEADER:
    {
        // ESP_LOGI("Download", "HTTP_EVENT_ON_HEADER");
    }
    break;

    case HTTP_EVENT_ON_DATA:
    {
        // ESP_LOGI("Download", "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        memcpy(arquivo_buffer + len, (char *)evt->data, evt->data_len);
        len += evt->data_len;
    }
    break;

    case HTTP_EVENT_ON_FINISH:
    {
        // ESP_LOGI("Download", "HTTP_EVENT_ON_FINISH");
    }
    break;

    case HTTP_EVENT_DISCONNECTED:
    {
        // ESP_LOGI("Download", "HTTP_EVENT_DISCONNECTED");
    }
    break;
    }
    return ESP_OK;
}

char *baixar_arquivo(char *url)
{
    arquivo_buffer = (char *)calloc(BUFFER_CERT, sizeof(char));
    esp_err_t err;
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 5000,
        .event_handler = _http_event_handle_download,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    if (client == NULL)
    {
        ESP_LOGE("Download", "Failed to initialise HTTP connection");
        task_fatal_error();
        free(arquivo_buffer);
        return NULL;
    }
    err = esp_http_client_perform(client);
    if (err != ESP_OK)
    {
        ESP_LOGE("Download", "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        task_fatal_error();
        free(arquivo_buffer);
        return NULL;
    }
    esp_http_client_cleanup(client);

    arquivo_buffer[len + 1] = '\0';
    char *arquivo_baixado = (char *)calloc(len + 1, sizeof(char));
    sprintf(arquivo_baixado, "%s", arquivo_buffer);
    free(arquivo_buffer);

    len = 0;

    return arquivo_baixado;
}