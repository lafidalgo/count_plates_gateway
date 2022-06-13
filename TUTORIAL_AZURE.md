## Geração dispositivo no portal Azure

- Instalação ESP-IDF : https://www.youtube.com/watch?v=S-N4MaOxyPA
- Dentro da pasta components, faça um clone do repositório gateway-common-esp32 e mude a branch para dev


- Para a geração dos ceritificados, garanta que as pastas _server\_certs_ e _spiffs\_certs_ só possuem o arquivo teste.txt
- Gere os certificados executando o script _config_cert.py_
- Na pasta _server\_certs_ estarão os arquivos _rootCA.key_ e _rootCA.pem_
- O terminal do python irá imprimir o COMMOM_NAME do certificado e do cliente
- Copie o COMMON_NAME do cliente, e crie um device no IotHub com esse nome
- Authentication type deve ser x.509_CA_Signed
- Salve, e selecione o dispositivo recém criado.
- Na aba Device Twin, adicione as seguintes propriedades no campo "desired"
-         "desired": {
            "quantidade_resets": {
                "crash": 0,
                "watchdog": 0,
                "software": 0,
                "inicioNormal": 0
            },
            "ble_address_gateway": "00FE",
            "baud_rate_gateway": 9600,
            "net_id_gateway": "1133",
            "versao_firmware": "1",
            "tipo_conexao": "wifi",
            "data_update_firmware": "2021-02-01T19:40:02.4089496Z 2343",
            "chave_update_firmware": "chave_teste 345",
            "config_rede": {
                "ip": "1.1.1.1",
                "gateway": "1.1.1.1",
                "netmask": "1.1.1.1",
                "DNS": "1.1.1.2",
                "ativo": "false",
                "nome_rede": "Orion_BYOD",
                "senha_rede": "CGe7X6xR",
                "usuario_gsm": "vivo"
            },
            "num_sensores": 1,
            "sensores": [
                {
                    "digitalTwinId": "SENSFLW-ED6B-6PV-VERA-CRUZ",
                    "bleMeshAddr": "0006"
                }
            ],

- Além disso, adicione o seguinte campo em cima de properties:
-       "tags": {
            "subject": "gateway_sensor_fluxo",
            "device_type": "GATEWAY_SENSOR_FLOW"
        },

- Crie um DPS cujo o ResourceGroup seja o do Iot Hub
- Entre no DPS->Certificates->Add. Adicione o arquivo _rootCA.pem_
- Nomeie o certificado com o COMMON_NAME do certificado
- Ainda dentro do DPS, vá em Manage Enrollments -> Add enrollment group
- Em primary Certificate escolha o certificado adicionado previamente
- Nas caixas de seleção marque _Evenly weighted distribution_ e o IoT Hub desejado
- Agora dentro da pasta do projeto, crie um arquivo na pasta spiffs_rede com o nome rede.txt
- deve ser do formato: 30,vivo.com.br,vivo,vivo,device_tishman_Uxpi1D61XFmFvBL,0ne0043DD63,
    - O primeiro campo é o tipo de conexao: 10 = wifi e 30 = gsm
    - Os campos seguintes sao Usuário e senha, caso seja wifi. Ou APN, usuario e senha, caso seja GSM.
    - O penúltimo campo é o DeviceId
    - O ultimo campo é o IDScope, que pode ser obtido na aba _Overview_ do DPS