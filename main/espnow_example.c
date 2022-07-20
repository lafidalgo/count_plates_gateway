/* ESPNOW Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/*
   This example shows how to use ESPNOW.
   Prepare two device, one for sending ESPNOW data and another for receiving
   ESPNOW data.
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_now.h"
#include "esp_crc.h"
#include "espnow_example.h"
#include "macros.h"
#include "memoriaNVS.h"
#include "Iothub_DeviceTwin.h"

#define ESPNOW_MAXDELAY 512
#define ESPNOW_PMK "pmk1234567890123"

static const char *TAG = "espnow_example";

static xQueueHandle s_example_espnow_queue;

static void example_espnow_deinit(example_espnow_send_param_t *send_param);

/* ESPNOW sending or receiving callback function is called in WiFi task.
 * Users should not do lengthy operations from this task. Instead, post
 * necessary data to a queue and handle it from a lower priority task. */
static void example_espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    example_espnow_event_t evt;
    example_espnow_event_send_cb_t *send_cb = &evt.info.send_cb;

    if (mac_addr == NULL)
    {
        ESP_LOGE(TAG, "Send cb arg error");
        return;
    }

    evt.id = EXAMPLE_ESPNOW_SEND_CB;
    memcpy(send_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    send_cb->status = status;
    if (xQueueSend(s_example_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE)
    {
        ESP_LOGW(TAG, "Send send queue fail");
    }
}

static void example_espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    example_espnow_event_t evt;
    example_espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;

    if (mac_addr == NULL || data == NULL || len <= 0)
    {
        ESP_LOGE(TAG, "Receive cb arg error");
        return;
    }

    evt.id = EXAMPLE_ESPNOW_RECV_CB;
    memcpy(recv_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    recv_cb->data = malloc(len);
    if (recv_cb->data == NULL)
    {
        ESP_LOGE(TAG, "Malloc receive data fail");
        return;
    }
    memcpy(recv_cb->data, data, len);
    recv_cb->data_len = len;
    if (xQueueSend(s_example_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE)
    {
        ESP_LOGW(TAG, "Send receive queue fail");
        free(recv_cb->data);
    }
}

/* Parse received ESPNOW data. */
int example_espnow_data_parse(uint8_t *data, uint16_t data_len, uint8_t *state, int *magic, uint8_t *wifi_channel, float *weightGrams, float *quantityUnits, uint32_t *batVoltage)
{
    example_espnow_data_t *buf = (example_espnow_data_t *)data;
    uint16_t crc, crc_cal = 0;

    if (data_len < sizeof(example_espnow_data_t))
    {
        ESP_LOGE(TAG, "Receive ESPNOW data too short, len:%d", data_len);
        return -1;
    }

    *wifi_channel = buf->wifi_channel;
    *weightGrams = buf->weightGrams;
    *quantityUnits = buf->quantityUnits;
    *batVoltage = buf->batVoltage;
    crc = buf->crc;
    buf->crc = 0;
    crc_cal = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, data_len);

    if (crc_cal == crc)
    {
        return buf->type;
    }

    return -1;
}

/* Prepare ESPNOW data to be sent. */
void example_espnow_data_prepare(example_espnow_send_param_t *send_param, int type, int index)
{
    example_espnow_data_t *buf = (example_espnow_data_t *)send_param->buffer;

    buf->type = type;
    buf->crc = 0;

    buf->is_ack = true;
    buf->weightGrams = 0;
    buf->quantityUnits = 0;
    buf->batVoltage = 0;

    buf->weightReference = deviceTwinTempoExecucao->sensor[index].weightReference;
    buf->repMeasureQtd = deviceTwinTempoExecucao->sensor[index].repMeasureQtd;
    buf->ulpWakeUpPeriod = deviceTwinTempoExecucao->sensor[index].ulpWakeUpPeriod;
    buf->heartbeatPeriod = deviceTwinTempoExecucao->sensor[index].heartbeatPeriod;
    buf->repEspNowSend = deviceTwinTempoExecucao->sensor[index].repEspNowSend;

    buf->crc = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, send_param->len);
}

void example_espnow_task(void *pvParameter)
{
    example_espnow_event_t evt;
    example_espnow_send_param_t *send_param;
    uint8_t recv_state = 0;
    int recv_magic = 0;
    uint8_t recv_wifi_channel = 1;
    float recv_weightGrams = 0;
    float recv_quantityUnits = 0;
    uint32_t recv_batVoltage = 0;
    int ret;
    int type = 0;

    /*ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(ESPNOW_WIFI_MODE));
    ESP_ERROR_CHECK(esp_wifi_start());*/

#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
    ESP_ERROR_CHECK(esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));
#endif

    s_example_espnow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(example_espnow_event_t));
    if (s_example_espnow_queue == NULL)
    {
        ESP_LOGE(TAG, "Create mutex fail");
        return ESP_FAIL;
    }

    /* Initialize ESPNOW and register sending and receiving callback function. */
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(example_espnow_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(example_espnow_recv_cb));

    /* Set primary master key. */
    ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *)ESPNOW_PMK));

    while (xQueueReceive(s_example_espnow_queue, &evt, portMAX_DELAY) == pdTRUE)
    {
        switch (evt.id)
        {
        case EXAMPLE_ESPNOW_SEND_CB:
        {
            example_espnow_event_send_cb_t *send_cb = &evt.info.send_cb;

            ESP_LOGI(TAG, "Send data to " MACSTR ", status: %d", MAC2STR(send_cb->mac_addr), send_cb->status);

            break;
        }
        case EXAMPLE_ESPNOW_RECV_CB:
        {
            example_espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;
            bool deviceInList = false;
            char macAddress[18];
            int index = 0;

            ret = example_espnow_data_parse(recv_cb->data, recv_cb->data_len, &recv_state, &recv_magic, &recv_wifi_channel, &recv_weightGrams, &recv_quantityUnits, &recv_batVoltage);
            free(recv_cb->data);

            /* Check if device is in sensor list */
            for (index = 0; index < deviceTwinTempoExecucao->num_sensores; index++)
            {
                sprintf(macAddress, MACSTR, MAC2STR(recv_cb->mac_addr));
                if (strcmp(deviceTwinTempoExecucao->sensor[index].macAddress, macAddress) == 0)
                {
                    deviceInList = true;

                    /* If MAC address does not exist in peer list, add it to peer list. */
                    if (esp_now_is_peer_exist(recv_cb->mac_addr) == false)
                    {
                        esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
                        if (peer == NULL)
                        {
                            ESP_LOGE(TAG, "Malloc peer information fail");
                            vSemaphoreDelete(s_example_espnow_queue);
                            esp_now_deinit();
                            vTaskDelete(NULL);
                        }
                        memset(peer, 0, sizeof(esp_now_peer_info_t));
                        peer->channel = recv_wifi_channel;
                        peer->ifidx = ESP_IF_WIFI_STA;
                        peer->encrypt = false;
                        // memcpy(peer->lmk, CONFIG_ESPNOW_LMK, ESP_NOW_KEY_LEN);
                        memcpy(peer->peer_addr, recv_cb->mac_addr, ESP_NOW_ETH_ALEN);
                        ESP_ERROR_CHECK(esp_now_add_peer(peer));
                        free(peer);
                    }
                    else
                    {
                        ESP_LOGW(TAG, "Device " MACSTR " already paired.", MAC2STR(recv_cb->mac_addr));
                    }
                    break;
                }
            }

            if (ret == EXAMPLE_ESPNOW_DATA_SEND)
            {
                ESP_LOGI(TAG, "Received send data from: " MACSTR ", len: %d, channel: %d, payload: %f\t%f\t%d", MAC2STR(recv_cb->mac_addr), recv_cb->data_len, recv_wifi_channel, recv_weightGrams, recv_quantityUnits, recv_batVoltage);
                type = EXAMPLE_ESPNOW_DATA_SEND;
            }
            else if (ret == EXAMPLE_ESPNOW_DATA_HEARTBEAT)
            {
                ESP_LOGI(TAG, "Received heartbeat data from: " MACSTR ", len: %d, channel: %d, payload: %f\t%f\t%d", MAC2STR(recv_cb->mac_addr), recv_cb->data_len, recv_wifi_channel, recv_weightGrams, recv_quantityUnits, recv_batVoltage);
                type = EXAMPLE_ESPNOW_DATA_HEARTBEAT;
            }
            else if (ret == EXAMPLE_ESPNOW_DATA_RESET)
            {
                bool deviceInList = false;
                ESP_LOGI(TAG, "Received reset data from: " MACSTR ", len: %d, channel: %d, payload: %f\t%f\t%d", MAC2STR(recv_cb->mac_addr), recv_cb->data_len, recv_wifi_channel, recv_weightGrams, recv_quantityUnits, recv_batVoltage);
                type = EXAMPLE_ESPNOW_DATA_RESET;
            }
            else
            {
                ESP_LOGI(TAG, "Received error data from: " MACSTR "", MAC2STR(recv_cb->mac_addr));
            }

            if (deviceInList)
            {
                // Pegando tempo em que envia para o back-end
                char *stringAgora = pegar_string_tempo_agora();
                if (xSemaphoreTake(xSemaphoreAcessoInternet, (TickType_t)100) == pdTRUE)
                {
                    adicionar_mensagem_recebida(
                        stringAgora,
                        macAddress,
                        type,
                        recv_weightGrams,
                        recv_quantityUnits,
                        recv_batVoltage);
                    free(stringAgora);
                    xSemaphoreGive(xSemaphoreAcessoInternet);
                }
            }

            /* Send ACK to the device */
            if (deviceInList)
            {
                /* Initialize sending parameters. */
                send_param = malloc(sizeof(example_espnow_send_param_t));
                memset(send_param, 0, sizeof(example_espnow_send_param_t));
                if (send_param == NULL)
                {
                    ESP_LOGE(TAG, "Malloc send parameter fail");
                    vSemaphoreDelete(s_example_espnow_queue);
                    esp_now_deinit();
                    return ESP_FAIL;
                }
                send_param->len = sizeof(example_espnow_data_t);
                send_param->buffer = malloc(sizeof(example_espnow_data_t));
                if (send_param->buffer == NULL)
                {
                    ESP_LOGE(TAG, "Malloc send buffer fail");
                    free(send_param);
                    vSemaphoreDelete(s_example_espnow_queue);
                    esp_now_deinit();
                    return ESP_FAIL;
                }

                /* Start sending unicast ESPNOW data. */
                memcpy(send_param->dest_mac, recv_cb->mac_addr, ESP_NOW_ETH_ALEN);
                example_espnow_data_prepare(send_param, type, index);
                // ESP_LOGI(TAG, "Send data to " MACSTR "", MAC2STR(recv_cb->mac_addr));
                if (esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len) != ESP_OK)
                {
                    ESP_LOGE(TAG, "Send error");
                    example_espnow_deinit(send_param);
                    vTaskDelete(NULL);
                }

                ESP_ERROR_CHECK(esp_now_del_peer(recv_cb->mac_addr));
            }
            else
            {
                ESP_LOGW(TAG, "Device " MACSTR " isn't paired yet.", MAC2STR(recv_cb->mac_addr));
            }
            break;
        }
        default:
            ESP_LOGE(TAG, "Callback type error: %d", evt.id);
            break;
        }
    }
}

static void example_espnow_deinit(example_espnow_send_param_t *send_param)
{
    free(send_param->buffer);
    free(send_param);
    vSemaphoreDelete(s_example_espnow_queue);
    esp_now_deinit();
}