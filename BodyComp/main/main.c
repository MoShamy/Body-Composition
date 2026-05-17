#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bia.h"
#include "ble_service.h"
#include "hcsr04.h"
#include "hx711.h"
#include "measurement_manager.h"

// Temporary demo profile until the mobile app is wired in.
#define DEFAULT_USER_AGE  22
#define DEFAULT_USER_SEX   BIA_SEX_MALE

void app_main(void)
{
    hx711_init();
    hcsr04_init();
    bia_init();
    // Allow DAC/ADC hardware to stabilize before taking measurements
    vTaskDelay(pdMS_TO_TICKS(50));

    ble_service_init();
    measurement_manager_init();

    ble_user_profile_t profile = {
        .age = DEFAULT_USER_AGE,
        .sex = DEFAULT_USER_SEX,
    };
    ble_service_set_user_profile(&profile);

    printf("HX711 ready. Taking weight readings...\n");
    printf("HC-SR04 ready. TRIG=GPIO%d, ECHO=GPIO%d\n", HCSR04_TRIG_PIN, HCSR04_ECHO_PIN);
    printf("Note: ECHO must be level-shifted to 3.3V before connecting to the ESP32.\n");
    printf("BIA ready. DAC sine on GPIO%d, ADC sense on GPIO%d.\n",
           BIA_DAC_GPIO, BIA_ADC_GPIO);

    printf("BLE control plane ready. Waiting for a start command from the mobile app.\n");

    vTaskDelete(NULL);
}