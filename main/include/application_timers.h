#ifndef APPLICATION_TIMER_H
#define APPLICATION_TIMER_H

#include "application_timers.h"
#include <string.h>
#include "wifi_setup.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
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
#include "azure_c_shared_utility/threadapi.h"
#include "verifica_status_conexao.h"
#include "Iothub_DeviceTwin.h"

#define TIMER_DIVIDER         16  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define TIMER_INTERVAL1_SEC   (30.0)   // TIMER wifi
#define TIMER_INTERVAL0_SEC   (300.0)   // timer para ajustar o clock com ntp
#define TEST_WITHOUT_RELOAD   0        // testing will be done without auto reload
#define TEST_WITH_RELOAD      1        // testing will be done with auto reload


typedef struct {
    int type;  // the type of timer's event
    int timer_group;
    int timer_idx;
    uint64_t timer_counter_value;
    int status_conexao;
    int clocksync_status;
} timer_event_t;

xQueueHandle timer_queue;

void IRAM_ATTR timer_group0_isr(void *);
void inicializa_timer(int , bool, double);
void task_timers(void *);


#endif
