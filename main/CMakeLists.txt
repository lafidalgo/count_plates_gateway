set(COMPONENT_SRCS "espnow_example.c" "Iothub_Mensagens.c" "crc.c" "application_timers.c" "macros.c" 
"updateClock.c"
"Iothub_DeviceTwin.c"
"memoriaNVS.c"
"crc.c"
"main.c"
INCLUDE_DIRS "."
INCLUDE_DIRS "include")

register_component()

component_compile_definitions(SET_TRUSTED_CERT_IN_SAMPLES)
spiffs_create_partition_image(storage_certs ../spiffs_certs FLASH_IN_PROJECT)
spiffs_create_partition_image(storage_rede ../spiffs_rede FLASH_IN_PROJECT)