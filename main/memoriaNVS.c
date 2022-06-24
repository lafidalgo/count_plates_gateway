#include "memoriaNVS.h"
#include "macros.h"

// DEFINICOES DOS TAMANHOS DAS MENSAGENS
#define QUANTIDADE_MENSAGENS 23
#define TAMANHO_MENSAGEM 150
#define TAMANHO_CABECALHO 150
#define TAMANHO_TEMP_DTID 50

#define CMP_HIST_IDX 1
#define CMP_HIST_SAID 2
#define CMP_HIST_ENTR 3
#define CMP_HIST_OBST 4
#define CMP_HIST_PERD 5
#define CMP_HIST_REPE 6
#define CMP_HIST_ECRC 7
#define CMP_HIST_REIN 8
#define CMP_HIST_HTBT 9

const char *cabecalho_spiffs = "id,tempo_envio,batidas_a_b,batidas_b_a,nivel_bateria,obstrucao,digital_twin_id\n";

int pegar_ultimo_id_backup(void)
{
    int i = 0;
    char linha[TAMANHO_MENSAGEM];
    // int CMP_ID = 1;

    FILE *arquivo_backup = abrir_arquivo_e_montar_particao("/spiffs/backup_mensagens.csv", "backup", "r");

    while (fgets(linha, (sizeof(linha)), arquivo_backup))
    {
        if (!is_str_equal(linha, (char *)cabecalho_spiffs))
        {
            i++;
        }
    }

    fechar_arquivo("backup", arquivo_backup);

    return i;
}

/*
    Escreve todas as mensagens que não foram enviadas ao Iothub
*/
int escrita_nvs_backup(char *tempo, uint32_t batidas_a_b, uint32_t batidas_b_a, uint8_t nivel_bateria, uint8_t obstrucao, char *digital_twin_id)
{
    int confirmacao = 0;
    int id_novo = 0;

    // id = pegar_ultimo_id_backup();
    id_novo++;

    FILE *arquivo_backup = abrir_arquivo_e_montar_particao("/spiffs/backup_mensagens.csv", "backup", "a");

    fprintf(arquivo_backup, "%d,%s,%d,%d,%d,%d,%s\n",
            id_novo,
            tempo,
            batidas_a_b,
            batidas_b_a,
            nivel_bateria,
            obstrucao,
            digital_twin_id);

    fechar_arquivo("backup", arquivo_backup);

    return confirmacao;
}

mensagem_backup_iothub *leitura_nvs_backup(int id)
{
    FILE *arquivo_backup = abrir_arquivo_e_montar_particao("/spiffs/backup_mensagens.csv", "backup", "r");
    char linha[TAMANHO_MENSAGEM];
    mensagem_backup_iothub *obj_mensagem = (mensagem_backup_iothub *)malloc(id * sizeof(mensagem_backup_iothub));
    int i = 0;

    while (fgets(linha, (sizeof(linha)), arquivo_backup))
    {
        if (!is_str_equal(linha, (char *)cabecalho_spiffs))
        {
            linha[strlen(linha) - 1] = '\0';

            obj_mensagem[i].tempo_envio = (char *)malloc(TAMANHO_TEMP_DTID * sizeof(char));
            obj_mensagem[i].digital_twin_id = (char *)malloc(TAMANHO_TEMP_DTID * sizeof(char));

            obj_mensagem[i].macAddress = (char *)malloc(TAMANHO_TEMP_DTID * sizeof(char));
            obj_mensagem[i].type = string_to_int(parse_field(linha, TYPE, ","));
            obj_mensagem[i].weightGrams = string_to_int(parse_field(linha, WEIGHT, ","));
            obj_mensagem[i].quantityUnits = string_to_int(parse_field(linha, QUANTITY, ","));
            obj_mensagem[i].batVoltage = string_to_int(parse_field(linha, VOLTAGE, ","));

            sprintf(obj_mensagem[i].tempo_envio, "%s", parse_field(linha, TEMPO_ENVIO, ","));
            sprintf(obj_mensagem[i].digital_twin_id, "%s", parse_field(linha, DIGITAL_TWIN, ","));

            i++;
        }
    }

    remove("/spiffs/backup_mensagens.csv");

    fechar_arquivo("backup", arquivo_backup);

    criar_arquivo_backup("backup", "/spiffs/backup_mensagens.csv");

    return obj_mensagem;
}

int init_arquivos_csv(void)
{
    FILE *arquivo_backup = abrir_arquivo_e_montar_particao("/spiffs/backup_mensagens.csv", "backup", "r");

    if (arquivo_backup == NULL)
    {
        fechar_arquivo("backup", arquivo_backup);

        criar_arquivo_backup("backup", "/spiffs/backup_mensagens.csv");
    }
    else
    {
        fechar_arquivo("backup", arquivo_backup);
    }

    FILE *arquivo_historico = abrir_arquivo_e_montar_particao("/spiffs/historico_sensores.csv", "historico", "r");

    if (arquivo_historico == NULL)
    {
        fechar_arquivo("historico", arquivo_historico);

        criar_arquivo_backup("historico", "/spiffs/historico_sensores.csv");
    }
    else
    {
        fechar_arquivo("historico", arquivo_historico);
    }

    return 1;
}

int escrita_mensagem_nao_enviada(mensagem_backup_iothub *chamadas_perdidas_backup, int id)
{
    FILE *arquivo_backup = abrir_arquivo_e_montar_particao("/spiffs/backup_mensagens.csv", "backup", "a");

    for (int i = 0; i < id; i++)
    {
        fprintf(arquivo_backup, "%d,%s,%s,%d,%f,%f,%d,%s\n",
                i + 1,
                chamadas_perdidas_backup[i].tempo_envio,
                chamadas_perdidas_backup[i].macAddress,
                chamadas_perdidas_backup[i].type,
                chamadas_perdidas_backup[i].weightGrams,
                chamadas_perdidas_backup[i].quantityUnits,
                chamadas_perdidas_backup[i].batVoltage,
                chamadas_perdidas_backup[i].digital_twin_id);
    }

    int confirmacao = fechar_arquivo("backup", arquivo_backup);

    return confirmacao;
}