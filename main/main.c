/*
 * LED blink with FreeRTOS
 */

#include "hardware/rtc.h"
#include "hardware/timer.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
#include <FreeRTOS.h>
#include <hardware/gpio.h>
#include <queue.h>
#include <semphr.h>
#include <task.h>
#include <time.h>

#include "gfx.h"
#include "ssd1306.h"

#include "pico/stdlib.h"
#include <stdio.h>

const uint BTN_1_OLED = 28;
const uint BTN_2_OLED = 26;
const uint BTN_3_OLED = 27;

const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;

#define TRIG_PIN 12
#define ECHO_PIN 13

SemaphoreHandle_t xSemaphore = NULL;
SemaphoreHandle_t xSemaphoreTimeDiff = NULL;

QueueHandle_t xQueue;

volatile bool alarm_fired = false;

int64_t alarm_callback(alarm_id_t id, void *user_data) {
    alarm_fired = true;

    return 0;
}

void oled1_btn_led_init(void) {
    gpio_init(LED_1_OLED);
    gpio_set_dir(LED_1_OLED, GPIO_OUT);

    gpio_init(LED_2_OLED);
    gpio_set_dir(LED_2_OLED, GPIO_OUT);

    gpio_init(LED_3_OLED);
    gpio_set_dir(LED_3_OLED, GPIO_OUT);

    gpio_init(BTN_1_OLED);
    gpio_set_dir(BTN_1_OLED, GPIO_IN);
    gpio_pull_up(BTN_1_OLED);

    gpio_init(BTN_2_OLED);
    gpio_set_dir(BTN_2_OLED, GPIO_IN);
    gpio_pull_up(BTN_2_OLED);

    gpio_init(BTN_3_OLED);
    gpio_set_dir(BTN_3_OLED, GPIO_IN);
    gpio_pull_up(BTN_3_OLED);
}

void pin_callback(uint gpio, uint32_t events) {
    static uint32_t time_start, time_end;
    if (events == GPIO_IRQ_EDGE_RISE) {
        time_start = time_us_32();
    } else if (events == GPIO_IRQ_EDGE_FALL) {
        time_end = time_us_32();
        uint32_t time_diff = time_end - time_start;
        xQueueReset(xQueue);
        xQueueSendFromISR(xQueue, &time_diff, NULL);
    }
}

void trigger_task(void *pvParameters) {
    while (1) {
        gpio_put(TRIG_PIN, 1);
        sleep_us(10);
        gpio_put(TRIG_PIN, 0);

        // Define um alarme para daqui a 300 milissegundos
        alarm_fired = false;
        add_alarm_in_ms(300, alarm_callback, NULL, false);

        vTaskDelay(pdMS_TO_TICKS(60));
    }
}

void oled_task(void *pvParameters) {
    printf("Inicializando Driver\n");
    ssd1306_init();

    printf("Inicializando GLX\n");
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    printf("Inicializando btn and LEDs\n");
    oled1_btn_led_init();

    // Inicializa o RTC e define a data e hora
    datetime_t t = {
        .year = 2024,
        .month = 03,
        .day = 04,
        .dotw = 3, // 0 é domingo, então 3 é quarta-feira
        .hour = 14,
        .min = 50,
        .sec = 00};
    rtc_init();
    rtc_set_datetime(&t);

    while (1) {
        char str[20], time_str[20], progress_str[34];
        uint32_t time_diff;
        xQueueReceive(xQueue, &time_diff, portMAX_DELAY);
        if (time_diff == -1) {
            strcpy(str, "Distancia: ERRO");
        } else {
            int distance = time_diff / 58;
            sprintf(str, "Distancia: %d cm", distance);

            // Cria a barra de progresso
            int progress = (distance > 30) ? 30 : distance;
            memset(progress_str, '#', progress);
            progress_str[progress] = '\0';
        }

        // Obtém a hora atual do RTC
        rtc_get_datetime(&t);
        sprintf(time_str, "%02d:%02d:%02d", t.hour, t.min, t.sec);

        gfx_clear_buffer(&disp);
        gfx_draw_string(&disp, 0, 0, 1, str);
        gfx_draw_string(&disp, 0, 10, 1, time_str);
        gfx_draw_string(&disp, 0, 20, 1, progress_str);
        printf("%s\n", str);
        printf("%s\n", time_str);
        printf("%s\n", progress_str);
        vTaskDelay(pdMS_TO_TICKS(1000));
        gfx_show(&disp);
        xQueueReset(xQueue);
    }
}

int main() {
    stdio_init_all();
    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);
    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &pin_callback);

    xSemaphore = xSemaphoreCreateBinary();
    xQueue = xQueueCreate(1, sizeof(uint32_t));

    xTaskCreate(trigger_task, "Trigger", 8190, NULL, 1, NULL);
    xTaskCreate(oled_task, "OLED", 8190, NULL, 1, NULL);

    vTaskStartScheduler();
    printf("Error al iniciar el scheduler\n");

    while (1) {
    }
}