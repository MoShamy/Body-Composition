#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"

// DAC sine output -> R1 -> LM358 V2I -> R3 -> red electrode (right wrist)
#define BIA_DAC_GPIO        GPIO_NUM_25     // ESP32 DAC1 (only GPIO25/26 support DAC)

// AD620 OUT -> ESP32 ADC1_CH6
#define BIA_ADC_GPIO        GPIO_NUM_34

#define BIA_FREQ_HZ         50000

typedef enum {
    BIA_SEX_MALE = 1,
    BIA_SEX_FEMALE = 0,
} bia_sex_t;

void bia_init(void);

// Acquires one ADC frame, runs Goertzel at 50kHz, and returns |Z| in ohms.
// Returns false on ADC read timeout.
bool bia_measure_impedance(float *z_ohms);

// Takes 20 raw readings, drops the 5 lowest and 5 highest, returns the mean of
// the middle 10.  Returns false if more than 4 of the 20 reads failed.
bool bia_measure_impedance_avg(float *z_ohms);

// Deurenberg single-frequency BIA equation.
//   height_cm, weight_kg, age in years, sex per bia_sex_t.
//   Returns body fat percentage [0..100].
float bia_estimate_body_fat_pct(float z_ohms, float height_cm, float weight_kg,
                                int age, bia_sex_t sex);
