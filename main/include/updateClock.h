#ifndef UPDATECLOCK_C
#define UPDATECLOCK_C

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "esp_sntp.h"

void sntp_sync_time(struct timeval*);
void time_sync_notification_cb(struct timeval*);
int updateTime(void);
void obtain_time(void);
void Configurar_sntp(void);

#endif