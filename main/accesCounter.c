/* UART Events Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <esp_task_wdt.h>
#include "accesCounter.h"
#include "macros.h"
#include "crc.h"
#include "Iothub_Mensagens.h"

static const char *TAG = "uart_events";
static int8_t jdy_watchdog_counter = 5;   // 5min para watchdog

/**
 * This example shows how to use the UART driver to handle special UART events.
 *
 * It also reads data from UART0 directly, and echoes it to console.
 *
 * - Port: UART0
 * - Receive (Rx) buffer: on
 * - Transmit (Tx) buffer: off
 * - Flow control: off
 * - Event queue: on
 * - Pin assignment: TxD (default), RxD (default)
 */

#define EX_UART_NUM UART_NUM_2
#define PATTERN_CHR_NUM    (1)         /*!< Set the number of consecutive and identical characters received by receiver which defines a UART pattern*/

#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)

#define TXD_PIN 19
#define RXD_PIN 18
#define BLE_PWR GPIO_NUM_23
#define LED_TST GPIO_NUM_13
#define GPIO_BIT_MASK  ((1ULL<<BLE_PWR) | (1ULL<<LED_TST)) 

#define SIZE_DIGITAL_TWIN 30

static QueueHandle_t uart1_queue;

static const char *cmd_ble_default = "AT+DEFAULT\r\n";
static const char *cmd_ble_role5 = "AT+ROLE5\r\n";
char *cmd_ble_netid;
char *cmd_ble_maddr;
static const char *cmd_ble_reset = "AT+RESET\r\n";
// static const char *cmd_ble_sleep = "AT+SLEEP2\r\n";

historico_repeticao historico;

void pins_init(void)
{
    gpio_config_t io_config = {
        .intr_type      =   GPIO_INTR_DISABLE,
        .mode           =   GPIO_MODE_OUTPUT,
        .pin_bit_mask   =   GPIO_BIT_MASK,
        .pull_down_en   =   GPIO_PULLDOWN_DISABLE,
        .pull_up_en     =   GPIO_PULLUP_DISABLE,
    };
    gpio_config(&io_config);

    gpio_set_level(BLE_PWR, pdTRUE);
}

void uart_init(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = deviceTwinTempoExecucao->baud_rate_gateway,
        // .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    //Install UART driver, and get the queue.
    uart_driver_install(EX_UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart1_queue, 0);
    uart_param_config(EX_UART_NUM, &uart_config);

    //Set UART log level
    esp_log_level_set(TAG, ESP_LOG_INFO);
    //Set UART pins (using UART0 default pins ie no changes.)
    uart_set_pin(EX_UART_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    //Set uart pattern detect function.
    // uart_enable_pattern_det_baud_intr(EX_UART_NUM, 0x0a, PATTERN_CHR_NUM, 9, 0, 0);
    //Reset the pattern queue length to record at most 20 pattern positions.
    // uart_pattern_queue_reset(EX_UART_NUM, 20);
}

void ble_config(void)
{
    uart_write_bytes(EX_UART_NUM, cmd_ble_default, strlen(cmd_ble_default));
    vTaskDelay(pdMS_TO_TICKS(1000));

    uart_write_bytes(EX_UART_NUM, cmd_ble_role5, strlen(cmd_ble_role5));
    vTaskDelay(pdMS_TO_TICKS(1000));

    cmd_ble_netid = (char *)malloc( sizeof(char)*SIZE_DIGITAL_TWIN );
    sprintf(cmd_ble_netid, "AT+NETID%s\r\n", deviceTwinTempoExecucao->net_id_gateway);

    uart_write_bytes(EX_UART_NUM, cmd_ble_netid, strlen(cmd_ble_netid));
    vTaskDelay(pdMS_TO_TICKS(1000));

    cmd_ble_maddr = (char *)malloc( sizeof(char)*SIZE_DIGITAL_TWIN );
    sprintf(cmd_ble_maddr, "AT+MADDR%s\r\n", deviceTwinTempoExecucao->ble_address_gateway);

    uart_write_bytes(EX_UART_NUM, cmd_ble_maddr, strlen(cmd_ble_maddr));
    vTaskDelay(pdMS_TO_TICKS(1000));

    uart_write_bytes(EX_UART_NUM, cmd_ble_reset, strlen(cmd_ble_reset));

    free(cmd_ble_netid);
    free(cmd_ble_maddr);
}


uint8_t ble_mesh_parse(const char* data)
{
    char* tokenHeader = strstr(data, "\xf1\xdd");
    ble_msg_t* p_rx_data = (ble_msg_t*)data;
    uint16_t msg_address_sender = 0;
    uint32_t new_saidas = 0;
    uint32_t new_entradas = 0;
    ble_payload_t payload = {0};
    int i = 0;
    char digital_twin_id[SIZE_DIGITAL_TWIN];

    if( tokenHeader != NULL )
    {
        msg_address_sender = p_rx_data->send_addr[0]<<8 | p_rx_data->send_addr[1];

        payload.cmd             = p_rx_data->payload[0];
        payload.total_entradas  = (p_rx_data->payload[1]<<16 | p_rx_data->payload[2]<<8 | p_rx_data->payload[3]) ;
        payload.total_saidas    = (p_rx_data->payload[4]<<16 | p_rx_data->payload[5]<<8 | p_rx_data->payload[6]) ;
        payload.battery         = p_rx_data->payload[7];
        payload.crc             = p_rx_data->payload[8]<<8 | p_rx_data->payload[9];

        crcInit();
        uint16_t crc_value = crcFast((const unsigned char *) p_rx_data->payload, 8);


        if(payload.cmd <= SENSOR_CMD_DOBSTR)
            i = achar_indice_sensor_por_endereco(msg_address_sender);
        else
            i = -1;

        if ( i == -1 )
        {
            printf("Digital Twin não encontrado para o dispositivo: %d\r\n", msg_address_sender);
            return 0;
        }
        sprintf(digital_twin_id, deviceTwinTempoExecucao->sensor[i].digitalTwinId);

        if(payload.crc != crc_value)
        {
            ESP_LOGE("CRC", "DIFERENTE");
            historico.historico_mensagens[i].erros_crc++;
            return 1;
        }
        
        new_entradas = payload.total_entradas - historico.historico_mensagens[i].old_entradas;
        new_saidas   = payload.total_saidas - historico.historico_mensagens[i].old_saidas;

        switch( payload.cmd )
        {
            case SENSOR_CMD_ACCES:
            {
                historico.historico_mensagens[i].obstrucao = 0;

                if( payload.total_entradas == historico.historico_mensagens[i].old_entradas )
                {
                    historico.historico_mensagens[i].msgs_repetidas++;
                    return 0;
                }
                else
                {
                    if (new_entradas < 1 || new_entradas > 10 || historico.historico_mensagens[i].old_entradas == 0) //Primeira entrada
                    {
                        new_entradas = 1;
                        new_saidas = 0;
                    }
                    else if(new_entradas > 1 && historico.historico_mensagens[i].old_entradas != 0) //Perdeu algum pacote entrada
                    {
                        historico.historico_mensagens[i].msgs_perdidas += new_entradas - 1;
                    }


                    if(new_saidas != 0) //Perdeu algum pacote saida
                    {
                        historico.historico_mensagens[i].msgs_perdidas += new_saidas;
                    }
                }
                historico.historico_mensagens[i].old_entradas = payload.total_entradas;
                historico.historico_mensagens[i].old_saidas = payload.total_saidas;
            }
            break;

            case SENSOR_CMD_EGRESS:
            {
                historico.historico_mensagens[i].obstrucao = 0;

                if( payload.total_saidas == historico.historico_mensagens[i].old_saidas )
                {
                    historico.historico_mensagens[i].msgs_repetidas++;
                    return 0;
                }
                else
                {
                    if (new_saidas < 1 || new_saidas > 10 || historico.historico_mensagens[i].old_saidas == 0) // Primeira saida
                    {
                        new_saidas = 1;
                        new_entradas = 0;
                    }
                    else if (new_saidas > 1 && historico.historico_mensagens[i].old_saidas != 0) // Perdeu algum pacote saida
                    {
                        historico.historico_mensagens[i].msgs_perdidas += new_saidas - 1;
                    }


                    if(new_entradas != 0) // Perdeu algum pacote entrada
                    {
                        historico.historico_mensagens[i].msgs_perdidas += new_entradas;
                    }
                }
                historico.historico_mensagens[i].old_entradas = payload.total_entradas;
                historico.historico_mensagens[i].old_saidas = payload.total_saidas;
            }
            break;

            case SENSOR_CMD_OBSTR:
            {
                new_entradas = 0;
                new_saidas = 0;
                if (historico.historico_mensagens[i].obstrucao == 0)
                {
                    historico.historico_mensagens[i].obstrucao = 1;
                }
                else
                {
                    return 0;
                }
                
            }
            break;

            case SENSOR_CMD_DOBSTR:
            {
                new_entradas = 0;
                new_saidas = 0;
                if (historico.historico_mensagens[i].obstrucao == 1)
                {
                    historico.historico_mensagens[i].obstrucao = 0;
                }
                else
                {
                    historico.historico_mensagens[i].msgs_repetidas++;
                    return 0;
                }
            }
            break;

            case SENSOR_CMD_INIT:
            {
                new_entradas = 0;
                new_saidas = 0;
                if(historico.historico_mensagens[i].reinicios != payload.total_entradas)
                {
                    historico.historico_mensagens[i].reinicios = payload.total_entradas;
                }
                else
                {
                    historico.historico_mensagens[i].msgs_repetidas++;
                    return 0;
                }
            }
            break;

            case SENSOR_CMD_HTBT:
            {
                new_entradas = 0;
                new_saidas = 0;
                if(historico.historico_mensagens[i].heart_beat != payload.total_saidas)
                {
                    historico.historico_mensagens[i].heart_beat = payload.total_saidas;
                }
                else
                {
                    historico.historico_mensagens[i].msgs_repetidas++;
                    return 0;
                }
            }
            break;
        }

        if(historico.historico_mensagens[i].old_saidas == 0 || historico.historico_mensagens[i].old_entradas == 0)
            return 0;

        // Pegando tempo em que envia para o back-end
        char *stringAgora = pegar_string_tempo_agora();
        ESP_LOGE("SENS", "MSGS REPETIDAS: %d", historico.historico_mensagens[i].msgs_repetidas);
        if (xSemaphoreTake(xSemaphoreAcessoInternet, (TickType_t)100) == pdTRUE)
        {
            adicionar_mensagem_recebida(
                stringAgora,
                historico.historico_mensagens[i].old_saidas,
                historico.historico_mensagens[i].old_entradas,
                payload.battery, 
                historico.historico_mensagens[i].obstrucao,
                historico.historico_mensagens[i].msgs_perdidas,
                historico.historico_mensagens[i].msgs_repetidas,
                historico.historico_mensagens[i].reinicios,
                historico.historico_mensagens[i].heart_beat,
                digital_twin_id
            );
            free(stringAgora);
            xSemaphoreGive(xSemaphoreAcessoInternet);
        }
        historico.historico_mensagens[i].msgs_repetidas = 0;
        quantidade_piscadas = 1;
    }
    else
    {
        ESP_LOGE("SENS", "Token incorreto da mensagem");
    }

    return 0;
}

void iniciar_simulador(void *pvParameters)
{
    while (1)
    {
        // char *simu_data = "\xf1\xdd\x09\x00\x03\x00\x66\x02\x00\x01\x12\x1e";
        char* simu_data = "\xf1\xdd\x0a\x00\x03\xff\xff\x01\x00\x00\xfe\x00\x00\x02\x30\xd6\x22";
        ble_mesh_parse(simu_data);
        vTaskDelay(5000/portTICK_PERIOD_MS);
    }
}

static inline void jdy_check_ping(void)
{
    uart_write_bytes(EX_UART_NUM, "AT\r\n", strlen("AT\r\n"));
}

void jdy_watchdog(void* pvParameteres)
{
    while(1){
        jdy_watchdog_counter--;
        json_watchdog_counter--;
        
        if(json_watchdog_counter == 0){
            ESP_LOGW("json_envio", "JSON TIMEOUT -> REBOOT");
            reinicia_salva_historico();
        }

        if(jdy_watchdog_counter == 0){
            jdy_check_ping();
        }
        else if(jdy_watchdog_counter == -1){    
            char *stringAgora = pegar_string_tempo_agora();
        
            if (xSemaphoreTake(xSemaphoreAcessoInternet, (TickType_t)100) == pdTRUE)
            {
                adicionar_mensagem_recebida(
                    stringAgora,
                    0, 
                    0,
                    0, 
                    255,
                    0,
                    0,
                    0,
                    0,
                    deviceTwinTempoExecucao->sensor[0].digitalTwinId
                );
                free(stringAgora);
                xSemaphoreGive(xSemaphoreAcessoInternet);
            }
        }

        vTaskDelay(60*1000/portTICK_PERIOD_MS);
    }

}


void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    size_t buffered_size;
    uint8_t* dtmp = (uint8_t*) malloc(RD_BUF_SIZE);
    for(;;) {
        //Waiting for UART event.
        if(xQueueReceive(uart1_queue, (void * )&event, (portTickType)portMAX_DELAY)) {
            bzero(dtmp, RD_BUF_SIZE);
            ESP_LOGI(TAG, "uart[%d] event:", EX_UART_NUM);
            switch(event.type) {
                //Event of UART receving data
                /*We'd better handler data event fast, there would be much more data events than
                other types of events. If we take too much time on data event, the queue might
                be full.*/
                case UART_DATA:
                    jdy_watchdog_counter = 5;
                    const int rxBytes = uart_read_bytes(EX_UART_NUM, dtmp, event.size, portMAX_DELAY);
                    ESP_LOG_BUFFER_HEXDUMP(TAG, dtmp, rxBytes, ESP_LOG_INFO);
                    ble_mesh_parse((char*)dtmp);
                    break;
                //Event of HW FIFO overflow detected
                case UART_FIFO_OVF:
                    ESP_LOGI(TAG, "hw fifo overflow");
                    // If fifo overflow happened, you should consider adding flow control for your application.
                    // The ISR has already reset the rx FIFO,
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(EX_UART_NUM);
                    xQueueReset(uart1_queue);
                    break;
                //Event of UART ring buffer full
                case UART_BUFFER_FULL:
                    ESP_LOGI(TAG, "ring buffer full");
                    // If buffer full happened, you should consider encreasing your buffer size
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(EX_UART_NUM);
                    xQueueReset(uart1_queue);
                    break;
                //Event of UART RX break detected
                case UART_BREAK:
                    ESP_LOGI(TAG, "uart rx break");
                    break;
                //Event of UART parity check error
                case UART_PARITY_ERR:
                    ESP_LOGI(TAG, "uart parity error");
                    break;
                //Event of UART frame error
                case UART_FRAME_ERR:
                    ESP_LOGI(TAG, "uart frame error");
                    break;
                //UART_PATTERN_DET
                case UART_PATTERN_DET:
                    uart_get_buffered_data_len(EX_UART_NUM, &buffered_size);
                    int pos = uart_pattern_pop_pos(EX_UART_NUM);
                    ESP_LOGI(TAG, "[UART PATTERN DETECTED] pos: %d, buffered size: %d", pos, buffered_size);
                    if (pos == -1) {
                        // There used to be a UART_PATTERN_DET event, but the pattern position queue is full so that it can not
                        // record the position. We should set a larger queue size.
                        // As an example, we directly flush the rx buffer here.
                        uart_flush_input(EX_UART_NUM);
                    } else {
                        uart_read_bytes(EX_UART_NUM, dtmp, pos, 100 / portTICK_PERIOD_MS);
                        uint8_t pat[PATTERN_CHR_NUM + 1];
                        memset(pat, 0, sizeof(pat));
                        uart_read_bytes(EX_UART_NUM, pat, PATTERN_CHR_NUM, 100 / portTICK_PERIOD_MS);
                        ESP_LOGI(TAG, "read data: %s", dtmp);
                        ESP_LOGI(TAG, "read pat : 0x%x", *pat);
                    }
                    break;
                //Others
                default:
                    ESP_LOGI(TAG, "uart event type: %d", event.type);
                    break;
            }
        }
        vTaskDelay(20/portTICK_PERIOD_MS);
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}
