#include "Iothub_Mensagens.h"
#include "memoriaNVS.h"
#include "macros.h"

#ifdef MBED_BUILD_TIMESTAMP
#define SET_TRUSTED_CERT_IN_SAMPLES
#endif // MBED_BUILD_TIMESTAMP

#ifdef SET_TRUSTED_CERT_IN_SAMPLES
#include "certs.h"
#endif // SET_TRUSTED_CERT_IN_SAMPLES

/*String containing Hostname, Device Id & Device Key in the format:                         */
/*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessKey=<device_key>"                */
/*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessSignature=<device_sas_token>"    */
#define EXAMPLE_IOTHUB_CONNECTION_STRING CONFIG_IOTHUB_CONNECTION_STRING
//static const char* connectionString = EXAMPLE_IOTHUB_CONNECTION_STRING;

// DEFINICOES DOS TAMANHOS DAS MENSAGENS
#define QUANTIDADE_MENSAGENS 15
#define TAMANHO_MENSAGEM 300
#define TAMANHO_CABECALHO 150
#define TAMANHO_TEMP_DTID 50

// Handle para comunicacao com o IotHub, gerado na thread de Device Twin
IOTHUB_DEVICE_CLIENT_LL_HANDLE iotHubClientHandle;
Device *deviceTwinTempoExecucao;

int8_t json_watchdog_counter = 15;   // 15min para watchdog
const char *TAGmsgIot = "msgParaIothub";
int contador_mensagens_perdidas = 0;
int callbackCounter;
char *msgText;
bool g_continueRunning;
int flag_status_conexao_hub;
objeto_mensagem_ram *mensagem_ram;

#define MESSAGE_COUNT CONFIG_MESSAGE_COUNT
#define DOWORK_LOOP_NUM 3

typedef struct EVENT_INSTANCE_TAG
{
    IOTHUB_MESSAGE_HANDLE messageHandle;
    size_t messageTrackingId; // For tracking the messages within the user callback.
} EVENT_INSTANCE;

IOTHUBMESSAGE_DISPOSITION_RESULT ReceiveMessageCallback(IOTHUB_MESSAGE_HANDLE message, void *userContextCallback)
{
    int *counter = (int *)userContextCallback;
    const char *buffer;
    size_t size;
    MAP_HANDLE mapProperties;
    const char *messageId;
    const char *correlationId;

    // Message properties
    if ((messageId = IoTHubMessage_GetMessageId(message)) == NULL)
    {
        messageId = "<null>";
    }

    if ((correlationId = IoTHubMessage_GetCorrelationId(message)) == NULL)
    {
        correlationId = "<null>";
    }

    // Message content
    if (IoTHubMessage_GetByteArray(message, (const unsigned char **)&buffer, &size) != IOTHUB_MESSAGE_OK)
    {
        ESP_LOGE("MSG", "unable to retrieve the message data");
    }
    else
    {
        ESP_LOGI("MSG", "Received Message [%d]\r\n Message ID: %s\r\n Correlation ID: %s\r\n Data: <<<%.*s>>> & Size=%d", *counter, messageId, correlationId, (int)size, buffer, (int)size);
        // If we receive the work 'quit' then we stop running
        if (size == (strlen("quit") * sizeof(char)) && memcmp(buffer, "quit", size) == 0)
        {
            g_continueRunning = false;
        }
    }

    // Retrieve properties from the message
    mapProperties = IoTHubMessage_Properties(message);
    if (mapProperties != NULL)
    {
        const char *const *keys;
        const char *const *values;
        size_t propertyCount = 0;
        if (Map_GetInternals(mapProperties, &keys, &values, &propertyCount) == MAP_OK)
        {
            if (propertyCount > 0)
            {
                size_t index;

                ESP_LOGI("MSG", " Message Properties:");
                for (index = 0; index < propertyCount; index++)
                {
                    ESP_LOGI("MSG", "\tKey: %s Value: %s", keys[index], values[index]);
                }
            }
        }

        free((char*) keys);
        free((char*) values);
    }

    /* Some device specific action code goes here... */
    free((char*) buffer);
    free((char*) messageId);
    free((char*) correlationId);
    (*counter)++;
    return IOTHUBMESSAGE_ACCEPTED;
}

void SendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *userContextCallback)
{
    EVENT_INSTANCE *eventInstance = (EVENT_INSTANCE *)userContextCallback;

    if (result == IOTHUB_CLIENT_CONFIRMATION_OK)
    {
        // (void)printf("Confirmation[%d] received for message tracking id = %d with result = %s\r\n", callbackCounter, (int)id, MU_ENUM_TO_STRING(IOTHUB_CLIENT_CONFIRMATION_RESULT, result));
        /* Some device specific action code goes here... */
        callbackCounter++;
        flag_status_conexao_hub = ATIVA; // atualiza a flag para confirmar o envio de mensagem
    }
    IoTHubMessage_Destroy(eventInstance->messageHandle);
}

//Aqui está o callbackd e mudança de status na conexão
void connection_status_callback(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void *userContextCallback)
{
    ESP_LOGI("MSG", "Connection Status result:%s, Connection Status reason: %s\n", MU_ENUM_TO_STRING(IOTHUB_CLIENT_CONNECTION_STATUS, result),
                 MU_ENUM_TO_STRING(IOTHUB_CLIENT_CONNECTION_STATUS_REASON, reason));
}

int montar_mensagem_iothub(
    mensagem_backup_iothub *chamadas_perdidas_backup, 
    int init, 
    int end, 
    char cabecalho[TAMANHO_CABECALHO]
)
{
    char array_json[TAMANHO_MENSAGEM];
    msgText[0] = '\0';
    strcat(msgText, cabecalho);
    float bateria;

    for (int i = init; i < end; i++)
    {
        sprintf(array_json, "{\"published\":\"%s\",\"type\":%d,\"weightGrams\":%.2f,\"quantityUnits\":%.2f,\"batVoltage\":%d}",
            chamadas_perdidas_backup[i].tempo_envio,
            chamadas_perdidas_backup[i].type,
            chamadas_perdidas_backup[i].weightGrams,
            chamadas_perdidas_backup[i].quantityUnits,
            chamadas_perdidas_backup[i].batVoltage
        );

        if (i != end-1)
        {
            strcat(array_json, ",");
        }

        strcat(msgText, array_json);
    }

    strcat(msgText, "]}");

    ESP_LOGW("json_envio", "%s", msgText);
    json_watchdog_counter = 15;

    return 1;
}

bool Enviar_Mensagem_Iothub(mensagem_backup_iothub *chamadas_perdidas_backup, int id, int verificadorConexao)
{
    //IOTHUB_CLIENT_LL_HANDLE iotHubClientHandleMessage;
    uint32_t heaps;
    EVENT_INSTANCE message;
    g_continueRunning = true;
    // alguns valores iniciais de variaveis so para testes
    callbackCounter = 0;
    IOTHUB_CLIENT_SAMPLE_INFO iothub_info;
    IOTHUB_CLIENT_STATUS status;
   
    if (verificadorConexao != 0) // parametro de verificacao
    {
        (void)printf("Failed to initialize the iothub message platform.\r\n");
        return false;
    }
    else
    {
        // Como o Handle eh criado pelo Device Twin, nao eh necessario criar outro aqui
        // nem chamar a funcao DoWork, que tambem eh chamada pela thread do Device Twin
        flag_status_conexao_hub = DESATIVADA;
        bool traceOn = false;
        int iterator = 0;

        int iteracao_batch = (int)((id/QUANTIDADE_MENSAGENS) + 1);
        int restante = id - ((iteracao_batch - 1)*QUANTIDADE_MENSAGENS);
        char cabecalho[TAMANHO_CABECALHO];

        if ( iteracao_batch == 1 )
        {
            msgText = (char *)malloc( sizeof(char) * (TAMANHO_CABECALHO + (restante * TAMANHO_MENSAGEM)) );
        }
        else
        {
            msgText = (char *)malloc( sizeof(char) * (TAMANHO_CABECALHO + (QUANTIDADE_MENSAGENS * TAMANHO_MENSAGEM)) );
        }

        char *tempoAgora = pegar_string_tempo_agora();
        sprintf(cabecalho,"{\"enqueuedTime\":\"%s\",\"tag\":\"esp-gateway-fluxo\",\"body\":[", tempoAgora);
        
        IoTHubClient_LL_SetOption(iotHubClientHandle, OPTION_LOG_TRACE, &traceOn);

        for (size_t i = 0; i < iteracao_batch; i++)
        {
            int min = i*QUANTIDADE_MENSAGENS;
            int max = min + QUANTIDADE_MENSAGENS;

            if (i == iteracao_batch - 1)
            {
                max = min + restante;
            }

            montar_mensagem_iothub(chamadas_perdidas_backup, min, max, cabecalho);
            
            IoTHubClient_LL_SetConnectionStatusCallback(iotHubClientHandle, connection_status_callback, &iothub_info);

            if ((message.messageHandle = IoTHubMessage_CreateFromByteArray((const unsigned char *)msgText, strlen(msgText))) == NULL)
            {
                free(tempoAgora);
                free(msgText);
                (void)printf("ERROR: iotHubMessageHandle is NULL!\r\n");
                contador_mensagens_perdidas++;

                return false;
            }
            else
            {
                // eh importante definir se algum parametro vai fazer parte das application properties
                // No momento nao eh usado, deixar comentado

                (void)IoTHubMessage_SetContentTypeSystemProperty(message.messageHandle, "application%2Fjson");
                (void)IoTHubMessage_SetContentEncodingSystemProperty(message.messageHandle, "utf-8");
                
                message.messageTrackingId = iterator;
                if (IoTHubClient_LL_SendEventAsync(iotHubClientHandle, message.messageHandle, SendConfirmationCallback, &message) != IOTHUB_CLIENT_OK)
                {
                    free(tempoAgora);
                    free(msgText);
                    (void)printf("ERROR: IoTHubClient_LL_SendEventAsync..........FAILED!\r\n");
                    contador_mensagens_perdidas++;

                    return false;
                }
                else
                {
                    // (void)printf("IoTHubClient_LL_SendEventAsync accepted message [%d] for transmission to IoT Hub.\r\n", (int)iterator);
                }
            }
            
            iterator++;
            while (
                IoTHubClient_LL_GetSendStatus(iotHubClientHandle, &status) == IOTHUB_CLIENT_OK &&
                status == IOTHUB_CLIENT_SEND_STATUS_BUSY
            )
            {
                vTaskDelay(50 / portTICK_PERIOD_MS);
            }

            if ( flag_status_conexao_hub != ATIVA ) {
                // mensagem nao foi enviada para o hub
                // espera 6 minutos para que o device twin possa fazer a reconexao com o hub
                free(tempoAgora);
                free(msgText);
                ESP_LOGE("MSG", "ERRO NO ENVIO\n");
                heaps = esp_get_free_heap_size();
                ESP_LOGI("MSG", "Free heap IotHub_Mensagens: %d", heaps);
                contador_mensagens_perdidas++;
                
                return false;
            }
        }

        free(tempoAgora);
        free(msgText);
        ESP_LOGI("MSG", "Iothub message has gotten quit message ...");
        contador_mensagens_perdidas = 0;

        heaps = esp_get_free_heap_size();
        ESP_LOGI("MSG", "Free heap IotHub_Mensagens: %d", heaps);
    }
    return true; // mensagem enviada com sucesso
}

bool iniciar_mensagem_recebida(void)
{
    mensagem_ram = (objeto_mensagem_ram *)malloc( sizeof(objeto_mensagem_ram) );
    mensagem_ram->mensagens_recebidas = (mensagem_backup_iothub *)malloc( sizeof(mensagem_backup_iothub) );
    mensagem_ram->quantidade = 0;

    return true;
}

bool adicionar_mensagem_recebida(char *tempo, int type, float weightGrams, float quantityUnits, uint32_t batVoltage)
{
    mensagem_ram->mensagens_recebidas = (mensagem_backup_iothub *)realloc(mensagem_ram->mensagens_recebidas, sizeof(mensagem_backup_iothub) * (mensagem_ram->quantidade + 1) );

    mensagem_ram->mensagens_recebidas[mensagem_ram->quantidade].tempo_envio = (char *)malloc( (strlen(tempo) + 1) * sizeof(char));

    mensagem_ram->mensagens_recebidas[mensagem_ram->quantidade].type = type;
    mensagem_ram->mensagens_recebidas[mensagem_ram->quantidade].weightGrams = weightGrams;
    mensagem_ram->mensagens_recebidas[mensagem_ram->quantidade].quantityUnits = quantityUnits;
    mensagem_ram->mensagens_recebidas[mensagem_ram->quantidade].batVoltage = batVoltage;

    strcpy(mensagem_ram->mensagens_recebidas[mensagem_ram->quantidade].tempo_envio, tempo);

    mensagem_ram->quantidade++;

    if (mensagem_ram->quantidade == QUANTIDADE_MENSAGENS - 1)
    {
        bool check = Enviar_Mensagem_Iothub(mensagem_ram->mensagens_recebidas, mensagem_ram->quantidade, 0);

        if (!check)
        {
            if (xSemaphoreTake(xSemaphoreAcessoSpiffs, (TickType_t)100) == pdTRUE)
            {
                escrita_mensagem_nao_enviada(mensagem_ram->mensagens_recebidas, mensagem_ram->quantidade);

                xSemaphoreGive(xSemaphoreAcessoSpiffs);
            }
        }
        
        limpar_mensagens_recebidas();
    }
    
    return true;
}

bool limpar_mensagens_recebidas(void)
{
    for (size_t i = 0; i < mensagem_ram->quantidade; i++)
    {
        free(mensagem_ram->mensagens_recebidas[i].tempo_envio);
        free(mensagem_ram->mensagens_recebidas[i].digital_twin_id);
    }

    free(mensagem_ram->mensagens_recebidas);
    free(mensagem_ram);
    
    iniciar_mensagem_recebida();

    return true;
}

objeto_mensagem_ram *pegar_mensagens(void)
{
    return mensagem_ram;
}