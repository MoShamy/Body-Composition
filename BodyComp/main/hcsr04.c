#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_err.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "hcsr04.h"

#define HCSR04_TIMER_RESOLUTION_HZ 1000000
#define HCSR04_PING_PERIOD_MS 60
#define HCSR04_ECHO_TIMEOUT_MS 40

typedef struct {
    uint32_t pulse_width_us;
    float distance_cm;
} hcsr04_measurement_t;

static gptimer_handle_t s_timer;
static QueueHandle_t s_measurement_queue;
static portMUX_TYPE s_echo_mux = portMUX_INITIALIZER_UNLOCKED;
static uint64_t s_echo_start_count;
static bool s_waiting_for_falling_edge;

static void hcsr04_set_echo_edge(gpio_int_type_t edge)
{
    gpio_set_intr_type(HCSR04_ECHO_PIN, edge);
}

static void IRAM_ATTR hcsr04_echo_isr_handler(void *arg)
{
    (void)arg;

    BaseType_t higher_priority_task_woken = pdFALSE;
    uint64_t count = 0;
    int level = gpio_get_level(HCSR04_ECHO_PIN);

    portENTER_CRITICAL_ISR(&s_echo_mux);

    if (level == 1 && !s_waiting_for_falling_edge) {
        if (gptimer_get_raw_count(s_timer, &count) == ESP_OK) {
            s_echo_start_count = count;
            s_waiting_for_falling_edge = true;
            hcsr04_set_echo_edge(GPIO_INTR_NEGEDGE);
        }
    } else if (level == 0 && s_waiting_for_falling_edge) {
        uint32_t pulse_width_us = 0;

        if (gptimer_get_raw_count(s_timer, &count) == ESP_OK) {
            pulse_width_us = (uint32_t)(count - s_echo_start_count);
            s_waiting_for_falling_edge = false;
            hcsr04_set_echo_edge(GPIO_INTR_POSEDGE);
            xQueueSendFromISR(s_measurement_queue, &pulse_width_us, &higher_priority_task_woken);
        }
    }

    portEXIT_CRITICAL_ISR(&s_echo_mux);

    if (higher_priority_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void hcsr04_trigger_pulse(void)
{
    gpio_set_level(HCSR04_TRIG_PIN, 0);
    esp_rom_delay_us(2);
    gpio_set_level(HCSR04_TRIG_PIN, 1);
    esp_rom_delay_us(10);
    gpio_set_level(HCSR04_TRIG_PIN, 0);
}

void hcsr04_init(void)
{
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = HCSR04_TIMER_RESOLUTION_HZ,
    };

    s_measurement_queue = xQueueCreate(4, sizeof(uint32_t));
    configASSERT(s_measurement_queue != NULL);

    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &s_timer));
    ESP_ERROR_CHECK(gptimer_enable(s_timer));
    ESP_ERROR_CHECK(gptimer_start(s_timer));

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << HCSR04_TRIG_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_set_level(HCSR04_TRIG_PIN, 0));

    io_conf.pin_bit_mask = (1ULL << HCSR04_ECHO_PIN);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    esp_err_t isr_service_err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (isr_service_err != ESP_OK && isr_service_err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(isr_service_err);
    }

    ESP_ERROR_CHECK(gpio_isr_handler_add(HCSR04_ECHO_PIN, hcsr04_echo_isr_handler, NULL));
}

bool hcsr04_read_pulse_width(uint32_t *pulse_width_us, TickType_t timeout_ticks)
{
    if (pulse_width_us == NULL) {
        return false;
    }

    portENTER_CRITICAL(&s_echo_mux);
    s_waiting_for_falling_edge = false;
    s_echo_start_count = 0;
    hcsr04_set_echo_edge(GPIO_INTR_POSEDGE);
    portEXIT_CRITICAL(&s_echo_mux);

    hcsr04_trigger_pulse();

    return xQueueReceive(s_measurement_queue, pulse_width_us, timeout_ticks) == pdTRUE;
}