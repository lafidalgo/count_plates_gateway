#include <string.h>
#include "macros.h"
#include "Iothub_Mensagens.h"
#include "Manipulacao_Arquivos.h"
#include "updateClock.h"
#include "application_timers.c"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_task_wdt.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "time.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_sntp.h"
#include "freertos/queue.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "Iothub_DeviceTwin.h"
#include "iothub.h"
#include "Manipulacao_Arquivos.h"
#include "freertos/semphr.h"
#include "memoriaNVS.h"
#include "verifica_status_conexao.h"
#include "atualizacao_ota.h"
#include "autenticacao_dps.h"
#include "wifi_setup.h"
#include "ethernet_setup.h"
#include "pppos_client.h"
#include "espnow_example.h"
// Variavel para os tipos de conexao
// Novo comentario
#define TAMANHO_CAMPOS_CFG          40

Device *deviceTwinTempoExecucao = NULL;
Device device_alocado;
char * rede_ip;
char * rede_gateway;
char * rede_netmask;
char * rede_dns;
char *rede_wifi_ssid;
char *rede_wifi_senha;
char *rede_gsm_apn;
char *rede_gsm_user;
char *rede_gsm_pass;
char *cert_common_name;
char *id_scope;

int tipo_conexao_ativa;
const char *TAGinicializacao = "Inicializacao Esp";
int verificador_ota = 0;

// strings para o uso do dps
char * dps_hub = NULL;
char * dps_id = NULL;

void verifica_rede(void)
{
    // verifica se existe o arquivo na memoria flash contendo a configuracao de rede do device twin
    // caso nao tenha, abre o arquivo de configuracao inicial de rede e usa para se conectar
    if( verifica_configuracoes_rede() == 0 )
    {
        // inicializa com rede device twin
        ESP_LOGI("Inicializacao de Rede", "Aplicando rede em memoria");
    }
    if(!device_connected)
    {
        usar_configs_fabrica = !usar_configs_fabrica;
        mudar_campo_usar_config_fabr(usar_configs_fabrica);   
    }
}

// Thread de comunicacao com o device twin
void azure_task(void *pvParameter)
{
    // int verificador;
    int cont = 0;
    
    memset(&device_alocado, 0, sizeof(Device));
    deviceTwinTempoExecucao = &device_alocado;
       
    ESP_LOGI(TAGtask, "Connected to AP success!");
    
    while ( 1 ) 
    {
        // if( cont !=0 )
        // {
        //     testa_nova_conexao();
        // }

        // verificador = checkConnectionStatus();

        // if( verificador == 1 && cont != 0) 
        // {
        //     // escreve as configuracoes de rede caso tenha se conectado com sucesso a internet
        //     // e nao seja a primeira conexao
        //     int status_novas_configs_rede;
        //     status_novas_configs_rede = escreve_configuracao_rede_em_memoria();
        //     if( status_novas_configs_rede == -1 ) 
        //     {
        //         ESP_LOGE("Spiffs", "ERRO AO SETAR NOVAS CONFIGS DE REDE EM MEMORIA FLASH");
        //     }
        //     else 
        //     {
        //         ESP_LOGI("Spiffs", "NOVAS CONFIGURACOES DE REDE ATUALIZADAS EM MEMORIA FLASH");
        //     }
        // }
        // else if(cont != 0) 
        // {
        //     ESP_LOGI("Verificacao de Rede", "Voltando para rede salva em memoria . . . ");
        //     if( tipo_conexao_ativa == WIFI )
        //         desliga_wifi();
        //     if( tipo_conexao_ativa == ETHERNET )
        //         desativa_ethernet();
        //     verifica_configuracoes_rede();
        // }
        InicializarIothubDeviceTwin(*((uint8_t*)pvParameter), dps_hub, dps_id);
        cont++;
        ESP_LOGI("MAIN", "Chamou a azure task %d vezes", cont);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
    
    vTaskDelete(NULL);
}

uint8_t Iniciar_Esp(void)
{
    ESP_LOGI(TAGinicializacao, "ESP_INIT");

    // cria o loop para poder usar as funcoes de wifi e ethernet
    ESP_LOGI(TAGinicializacao, "Inicializando configuracoes de rede");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Aloca memoria para as variaveis contendo as configuracoes da rede
    rede_ip = (char *)malloc( sizeof(char)*TAMANHO_CAMPOS_CFG );
    rede_netmask = (char *)malloc( sizeof(char)*TAMANHO_CAMPOS_CFG );
    rede_gateway = (char *)malloc( sizeof(char)*TAMANHO_CAMPOS_CFG );
    rede_dns = (char *)malloc( sizeof(char)*TAMANHO_CAMPOS_CFG );
    rede_wifi_ssid = (char *)malloc( sizeof(char)*TAMANHO_CAMPOS_CFG );
    rede_wifi_senha = (char *)malloc( sizeof(char)*TAMANHO_CAMPOS_CFG );
    cert_common_name = (char *)malloc( sizeof(char)*TAMANHO_CAMPOS_CFG );
    id_scope = (char *)malloc( sizeof(char)*TAMANHO_CAMPOS_CFG );

    Configurar_sntp(); // atualiza o servidor ntp para ser a pool brasileira
    // Inicializa LED e timers
    timer_queue = xQueueCreate(10, sizeof(timer_event_t));
    
    inicializa_timer(TIMER_1, TEST_WITH_RELOAD, TIMER_INTERVAL1_SEC); // timer wifi
    inicializa_timer(TIMER_0, TEST_WITH_RELOAD, TIMER_INTERVAL0_SEC); // timer clocksync
    ESP_LOGI(TAGinicializacao, "Led e Timers");

    // Inicializa semaforo do Device twin
    xSemaphoreDeviceTwin = xSemaphoreCreateBinaryStatic(&xSemaphoreBufferDeviceTwin);
    xSemaphoreGive(xSemaphoreDeviceTwin); // precisa dar a primeira vez para executar corretamente
    xSemaphoreAcessoInternet = xSemaphoreCreateBinaryStatic(&xSemaphoreBufferOtaUpdate);
    xSemaphoreGive(xSemaphoreAcessoInternet);

    iniciar_mensagem_recebida();
    init_arquivos_csv();

    xSemaphoreAcessoSpiffs = xSemaphoreCreateBinaryStatic(&xSemaphoreBufferAcessoNVS);
    xSemaphoreGive(xSemaphoreAcessoSpiffs);

    ESP_LOGI(TAGinicializacao, "Filas de Log");
    ESP_LOGI(TAGinicializacao, "Verificando status da internet");
    // importante verifica o sstatus da internet antes de chamar IoTHub_Init, se nao houver conexao
    // a funcao fica presa em um loop tentando atualizar o horario com o servidor NTP

    verificador_ota++;                  // adiciona uma inicializacao concluida com sucesso

    // esp_task_wdt_init(60, true); //Inicia o Task WDT com 1min

    return 0;
}

// envio das mensagens a partir do backup
void enviar_msg_iothub(void *pvParameter)
{
    bool verificaStatusMensagem;
    int id = 0;

    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    // esp_task_wdt_add(NULL);
    while (1)
    {
        // esp_task_wdt_reset();//Alimenta o WDT
        if (xSemaphoreTake(xSemaphoreAcessoSpiffs, (TickType_t)100) == pdTRUE)
        {
            id = pegar_ultimo_id_backup();

            if ( id > 0 )
            {
                mensagem_backup_iothub *chamadas_perdidas_backup = leitura_nvs_backup(id);

                if (xSemaphoreTake(xSemaphoreAcessoInternet, (TickType_t)100) == pdTRUE)
                {
                    ESP_LOGI("MSG", "ENVIANDO MENSAGEM BACKUP . . .");
                    
                    // nova funcao para enviar mensagem ao iothub, com as novas informacoes
                    verificaStatusMensagem = Enviar_Mensagem_Iothub(chamadas_perdidas_backup, id, 0);
                
                    if (!verificaStatusMensagem)
                    {
                        xSemaphoreGive(xSemaphoreAcessoInternet);

                        
                        escrita_mensagem_nao_enviada(chamadas_perdidas_backup, id);
                        xSemaphoreGive(xSemaphoreAcessoSpiffs);

                        for (size_t i = 0; i < id; i++)
                        {
                            free(chamadas_perdidas_backup[i].tempo_envio);
                            free(chamadas_perdidas_backup[i].digital_twin_id);
                        }
                        free(chamadas_perdidas_backup);
                        vTaskDelay(2000 / portTICK_PERIOD_MS);
                    }
                    else
                    {
                        xSemaphoreGive(xSemaphoreAcessoSpiffs);
                        for (size_t i = 0; i < id; i++)
                        {
                            free(chamadas_perdidas_backup[i].tempo_envio);
                            free(chamadas_perdidas_backup[i].digital_twin_id);
                        }
                        free(chamadas_perdidas_backup);
                        xSemaphoreGive(xSemaphoreAcessoInternet);
                    }
                }
            }
            else
            {
                xSemaphoreGive(xSemaphoreAcessoSpiffs);
            }
        }

        if (xSemaphoreTake(xSemaphoreAcessoInternet, (TickType_t)100) == pdTRUE)
        {
            objeto_mensagem_ram *mensagens = pegar_mensagens();

            if (mensagens->quantidade > 0)
            {
                ESP_LOGI("MSG", "ENVIANDO MENSAGEM RECEBIDA . . .");
                
                // nova funcao para enviar mensagem ao iothub, com as novas informacoes
                verificaStatusMensagem = Enviar_Mensagem_Iothub(mensagens->mensagens_recebidas, mensagens->quantidade, 0);
            
                if (!verificaStatusMensagem)
                {
                    if (xSemaphoreTake(xSemaphoreAcessoSpiffs, (TickType_t)100) == pdTRUE)
                    {
                        escrita_mensagem_nao_enviada(mensagens->mensagens_recebidas, mensagens->quantidade);

                        xSemaphoreGive(xSemaphoreAcessoSpiffs);
                    }

                    limpar_mensagens_recebidas();
                    xSemaphoreGive(xSemaphoreAcessoInternet);
                    vTaskDelay(2000 / portTICK_PERIOD_MS);
                }
                else
                {
                    limpar_mensagens_recebidas();
                    xSemaphoreGive(xSemaphoreAcessoInternet);
                }
            }
            else
            {
                limpar_mensagens_recebidas();
                xSemaphoreGive(xSemaphoreAcessoInternet);
            }
        }

        if ( contador_mensagens_perdidas > 10 )
        {
            esp_restart();
        }

        vTaskDelay(750 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    uint8_t erro_init = 0;
    Informacoes_Dispositivo_Dps dps_aut;
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    uint8_t verificador_iothub;
    
    verificador_iothub = Iniciar_Esp(); // faz a inicializacao dos componentes

    if (xTaskCreatePinnedToCore(led_status_task, "led_status_task", 2*1024, NULL, 1, NULL, 1) != pdPASS)
    {
        ESP_LOGE(TAGtask, "led task fail");
        erro_init = 1;
    }

    while(!device_connected)
    {
        verifica_rede();    // Tenta diferentes configs ate conectar
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
    dps_aut = provisiona_dps();
    // alocando as strings do dps
    if (dps_aut.iothub_uri == NULL)
    {
        while (1)
        {
            ESP_LOGE("DPS", "Conexão ao DPS negada!");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
        }
    }
    
    dps_hub = malloc( (strlen(dps_aut.iothub_uri)+1) * sizeof(char));
    dps_id = malloc( (strlen(dps_aut.device_id)+1) * sizeof(char));
    strcpy(dps_hub, dps_aut.iothub_uri);
    strcpy(dps_id, dps_aut.device_id);

    //thread para verificar atualizacoes no device twin
    if (xTaskCreatePinnedToCore(azure_task, "azure_task", 1024 * 12, &verificador_iothub, 3, NULL, 1) != pdPASS)
    {
        ESP_LOGE(TAGtask, "Task azure fail");
        erro_init = 1;
    }
     
    // Verifica se conseguiu conectar com o device twin, caso nao tenha conseguido, o esp deve reiniciar
    int status_devT = 0;
    while (verifica_status_device_twin() == 0)
    {
        ESP_LOGI(TAGtask, "Aguardando Device Twin ...");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        status_devT++;
        if (status_devT == 20)
        {
            ESP_LOGE(TAGtask, "Device Twin nao conectou, reiniciando...");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            esp_restart();
        }
    }

    // Apos a conexao com o device twin, testar para ver se inicilizou corretamente 
    status_devT = verifica_status_device_twin();
    verificador_ota += status_devT;
    // Realiza um auto diagnostico caso tenha sido feito um update OTA, para ver se as funcionalidades estao corretas
    // ou verificar se eh necessario um rollback
    verificacao_ota(verificador_ota);
    
    //task para os timers
    if (xTaskCreatePinnedToCore(task_timers, "timer_evt_task", 4096 * 2, NULL, 2, NULL, 1) != pdPASS)
    {
        ESP_LOGE(TAGtask, "Task timer fail");
        erro_init = 1;
    }

    // task para enviar mensagens ao iothub
    if (xTaskCreatePinnedToCore(enviar_msg_iothub, "enviar_msg_iothub", 30384, NULL, 15, NULL, 1) != pdPASS)
    {
        ESP_LOGE(TAGtask, "Task enviar_msg_iothub fail");
        erro_init = 1;
    }

    //Create a task to handler UART event from ISR
    if (xTaskCreatePinnedToCore(example_espnow_task, "example_espnow_task", 20480, NULL, configMAX_PRIORITIES-1, NULL, 0) != pdPASS)
    {
        ESP_LOGE(TAGtask, "ESP NOW fail");
        erro_init = 1;
    }

    // if (xTaskCreatePinnedToCore(iniciar_simulador, "simulador", 10240, NULL, 5, NULL, 1) != pdPASS)
    // {
    //     ESP_LOGE(TAGtask, "simu fail");
    // }
    if(!erro_init)
    {
        ESP_LOGI(TAGtask, "Gateway Inicializado");
        quantidade_piscadas = 5;
        velocidade_piscadas = PISCADA_RAPIDA;
    }
    else
    {
        ESP_LOGE(TAGtask, "Erro Init Gateway");
        quantidade_piscadas = 255;
        velocidade_piscadas = PISCADA_LENTA;
        vTaskDelay(10000/portTICK_PERIOD_MS);
        esp_restart();
    }
}
