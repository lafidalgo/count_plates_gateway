param($p1)
cls
echo "Setando o ambiente idf"
# Eh importante que o caminho para a pasta esp-idf esteja correto, depende de como foi feita a instalacao
# pode por exemplo, ser apenas .$HOME/esp-idf/export.ps1
.$HOME/esp/esp-idf/export.ps1
#echo "Resetando o repositorio caso ja tenha uma build feita"
#idf.py set-target esp32
echo "Dando build no projeto"
idf.py build
echo "Trocando de diretorio"
cd build
Connect-AzAccount
$storageAccount = Get-AzStorageAccount -ResourceGroupName "ORION-DEV-TEST" -Name "devicesorion"
$ctx = $storageAccount.Context
$containerName = "teste"
Set-AzStorageBlobContent -File "esp_orion.bin" -Container $containerName -Blob "$p1" -Context $ctx