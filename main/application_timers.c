#include "application_timers.h"
#include "esp_task_wdt.h"

#define TRESHOLD_PINGS_ERRO 8

void inline print_timer_counter(uint64_t counter_value)
{
    ESP_LOGI("TIMER", "Counter: 0x%08x%08x\n", (uint32_t) (counter_value >> 32),
           (uint32_t) (counter_value));
    ESP_LOGI("TIMER", "Time   : %.8f s\n", (double) counter_value / TIMER_SCALE);
}

/*
 * Timer group0 ISR handler
 *
 * Note:
 * We don't call the timer API here because they are not declared with IRAM_ATTR.
 * If we're okay with the timer irq not being serviced while SPI flash cache is disabled,
 * we can allocate this interrupt without the ESP_INTR_FLAG_IRAM flag and use the normal API.
 */
void IRAM_ATTR timer_group0_isr(void *para)
{
    timer_spinlock_take(TIMER_GROUP_0);
    int timer_idx = (int) para;

    /* Retrieve the interrupt status and the counter value
       from the timer that reported the interrupt */
    uint32_t timer_intr = timer_group_get_intr_status_in_isr(TIMER_GROUP_0);
    uint64_t timer_counter_value = timer_group_get_counter_value_in_isr(TIMER_GROUP_0, timer_idx);

    /* Prepare basic event data
       that will be then sent back to the main program task */
    timer_event_t evt;
    evt.timer_group = 0;
    evt.timer_idx = timer_idx;
    evt.timer_counter_value = timer_counter_value;
    if(timer_idx == TIMER_1) {
        evt.status_conexao = 1;
        evt.clocksync_status = 0;
    }
    if(timer_idx == TIMER_0) {
        evt.status_conexao = 0;
        evt.clocksync_status = 1;
    }

    

    /* Clear the interrupt
       and update the alarm time for the timer with without reload */

    if (timer_intr & TIMER_INTR_T0) {
        evt.type = TEST_WITH_RELOAD;
        timer_group_clr_intr_status_in_isr(TIMER_GROUP_0, TIMER_0);
    }   
    else if (timer_intr & TIMER_INTR_T1) {
        evt.type = TEST_WITH_RELOAD;
        timer_group_clr_intr_status_in_isr(TIMER_GROUP_0, TIMER_1);
    } else {
        evt.type = -1; // not supported even type
    }

    /* After the alarm has been triggered
      we need enable it again, so it is triggered the next time */
    timer_group_enable_alarm_in_isr(TIMER_GROUP_0, timer_idx);

    /* Now just send the event data back to the main program task */
    xQueueSendFromISR(timer_queue, &evt, NULL);
    timer_spinlock_give(TIMER_GROUP_0);
}

/*
 * Initialize selected timer of the timer group 0
 *
 * timer_idx - the timer number to initialize
 * auto_reload - should the timer auto reload on alarm?
 * timer_interval_sec - the interval of alarm to set
 */
void inicializa_timer(int timer_idx,
                                   bool auto_reload, double timer_interval_sec)
{
    /* Select and initialize basic parameters of the timer */
    timer_config_t config = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = auto_reload,
    }; // default clock source is APB
    timer_init(TIMER_GROUP_0, timer_idx, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(TIMER_GROUP_0, timer_idx, 0x00000000ULL);

    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(TIMER_GROUP_0, timer_idx, timer_interval_sec * TIMER_SCALE);
    timer_enable_intr(TIMER_GROUP_0, timer_idx);
    timer_isr_register(TIMER_GROUP_0, timer_idx, timer_group0_isr,
                       (void *) timer_idx, ESP_INTR_FLAG_IRAM, NULL);

    timer_start(TIMER_GROUP_0, timer_idx);
}


void task_timers(void *arg)
{
    /* Funcao que recebe os argumentos armazenados na lista. A interrupcao preenche essa lista */
    // esp_task_wdt_add(NULL);
    uint8_t contador_pings_erro = 0;    // Nao conseguiu muitos pings -> provavelmente perdeu conexao
    while (1)
    {
        // esp_task_wdt_reset();//Alimenta o WDT
        timer_event_t evt;
        xQueueReceive(timer_queue, &evt, portMAX_DELAY);
        if(evt.status_conexao == 1) {
            if (xSemaphoreTake(xSemaphoreAcessoInternet, (TickType_t)1000) == pdTRUE)
            {
                checkConnectionStatus();
                xSemaphoreGive(xSemaphoreAcessoInternet);
                contador_pings_erro = 0;
            }
            else
            {
                if(TRESHOLD_PINGS_ERRO == contador_pings_erro++)
                {
                    esp_restart();
                }
                ESP_LOGE("MSG", "SEM ACESSO SEMAFORO INTERNET -> contador pings erro:%d", contador_pings_erro);
            }
            evt.status_conexao = 0;
        }
        if(evt.clocksync_status == 1) {
            //updateTime();
            evt.clocksync_status = 0;
        }
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
