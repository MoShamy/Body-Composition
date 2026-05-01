#include "hx711.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"
#include "esp_log.h"

static const char *TAG = "hx711";

void hx711_init(void) {
    gpio_config_t io_conf = {};

    // SCK as output
    io_conf.pin_bit_mask = (1ULL << HX711_SCK_PIN);
    io_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io_conf);

    // DT as input
    io_conf.pin_bit_mask = (1ULL << HX711_DT_PIN);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    gpio_set_level(HX711_SCK_PIN, 0);
}

// Returns 1 if data is ready
int hx711_is_ready(void) {
    return gpio_get_level(HX711_DT_PIN) == 0;
}

static portMUX_TYPE hx711_mux = portMUX_INITIALIZER_UNLOCKED;

int32_t hx711_read(void) {
    // Wait for data ready (with timeout so we don't hang forever)
    int timeout_ticks = 100; // ~1 second at 100 Hz tick
    while (!hx711_is_ready()) {
        vTaskDelay(1);
        if (--timeout_ticks <= 0) {
            ESP_LOGW(TAG, "DT never went low — check wiring/power. DT=%d", gpio_get_level(HX711_DT_PIN));
            return 0;
        }
    }

    int32_t data = 0;

    // Bit-bang must not be preempted (HX711 powers down if SCK held >60us)
    portENTER_CRITICAL(&hx711_mux);

    for (int i = 0; i < 24; i++) {
        gpio_set_level(HX711_SCK_PIN, 1);
        ets_delay_us(1);
        data = (data << 1) | gpio_get_level(HX711_DT_PIN);
        gpio_set_level(HX711_SCK_PIN, 0);
        ets_delay_us(1);
    }

    // 25th pulse — sets gain to 128 for channel A
    gpio_set_level(HX711_SCK_PIN, 1);
    ets_delay_us(1);
    gpio_set_level(HX711_SCK_PIN, 0);

    portEXIT_CRITICAL(&hx711_mux);

    // Convert from 24-bit two's complement
    if (data & 0x800000) {
        data |= 0xFF000000;
    }

    return data;
}

float hx711_get_weight(int32_t offset, float scale) {
    int32_t raw = hx711_read();
    return (float)(raw - offset) / scale;
}