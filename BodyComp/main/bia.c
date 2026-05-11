#include "bia.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "driver/dac_cosine.h"
#include "esp_adc/adc_continuous.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BIA_SAMPLE_RATE_HZ   200000                 // 4x oversampling of the 50 kHz tone
#define BIA_FRAME_SAMPLES    1024                   // Goertzel analysis window

#define BIA_ADC_CHANNEL      ADC_CHANNEL_6          // GPIO34 on ESP32
#define BIA_ADC_UNIT         ADC_UNIT_1
#define BIA_ADC_ATTEN        ADC_ATTEN_DB_12        // ~0..3.1 V input range
#define BIA_ADC_BITWIDTH     ADC_BITWIDTH_12

// AD620 gain = 1 + 49.4k / Rg, with Rg = R6 = 2 kΩ
#define AD620_GAIN           25.7f

// V2I converter: I = V_DAC_AC / R2.  With ~3.3 Vpp DAC swing and R2 = 10 kΩ:
//   Vpp_DAC / R2 = 330 µA pp -> ~117 µA rms.
// MUST be re-tuned empirically against a known reference resistor (e.g. 500 Ω in
// place of the body) before any clinical interpretation.
#define INJECTION_CURRENT_RMS_A   5.67e-6f

#define ADC_VREF_V           3.3f
#define ADC_FULL_SCALE       4095.0f

static const char *TAG = "bia";

static dac_cosine_handle_t s_dac_cw;
static adc_continuous_handle_t s_adc;

void bia_init(void)
{
    dac_cosine_config_t cw_cfg = {
        .chan_id  = DAC_CHAN_0,                     // GPIO25
        .freq_hz  = BIA_FREQ_HZ,                    // 50 kHz hardware sine
        .clk_src  = DAC_COSINE_CLK_SRC_DEFAULT,
        .offset   = 0,
        .phase    = DAC_COSINE_PHASE_0,
        .atten    = DAC_COSINE_ATTEN_DEFAULT,
        .flags.force_set_freq = true,
    };
    ESP_ERROR_CHECK(dac_cosine_new_channel(&cw_cfg, &s_dac_cw));
    ESP_ERROR_CHECK(dac_cosine_start(s_dac_cw));

    adc_continuous_handle_cfg_t hcfg = {
        .max_store_buf_size = 4 * BIA_FRAME_SAMPLES * sizeof(uint16_t),
        .conv_frame_size    = BIA_FRAME_SAMPLES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&hcfg, &s_adc));

    adc_digi_pattern_config_t pat = {
        .atten     = BIA_ADC_ATTEN,
        .channel   = BIA_ADC_CHANNEL,
        .unit      = BIA_ADC_UNIT,
        .bit_width = BIA_ADC_BITWIDTH,
    };
    adc_continuous_config_t acfg = {
        .sample_freq_hz = BIA_SAMPLE_RATE_HZ,
        .conv_mode      = ADC_CONV_SINGLE_UNIT_1,
        .format         = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
        .pattern_num    = 1,
        .adc_pattern    = &pat,
    };
    ESP_ERROR_CHECK(adc_continuous_config(s_adc, &acfg));
    ESP_ERROR_CHECK(adc_continuous_start(s_adc));

    ESP_LOGI(TAG, "BIA ready: DAC cosine GPIO%d @ %d Hz, ADC=GPIO%d @ %d Hz",
             BIA_DAC_GPIO, BIA_FREQ_HZ, BIA_ADC_GPIO, BIA_SAMPLE_RATE_HZ);
}

// Single-bin DFT (Goertzel) — returns peak amplitude at ftarget in same units as x[].
static float goertzel_amplitude(const float *x, int n, float fs, float ftarget)
{
    float k     = ftarget * (float)n / fs;
    float omega = 2.0f * (float)M_PI * k / (float)n;
    float c     = cosf(omega);
    float s     = sinf(omega);
    float coeff = 2.0f * c;
    float q0, q1 = 0.0f, q2 = 0.0f;

    for (int i = 0; i < n; i++) {
        q0 = coeff * q1 - q2 + x[i];
        q2 = q1;
        q1 = q0;
    }

    float real = q1 - q2 * c;
    float imag = q2 * s;
    return sqrtf(real * real + imag * imag) * 2.0f / (float)n;
}

bool bia_measure_impedance(float *z_ohms)
{
    static uint8_t raw_buf[BIA_FRAME_SAMPLES * sizeof(adc_digi_output_data_t)];
    static float   samples[BIA_FRAME_SAMPLES];

    uint32_t out_len = 0;
    esp_err_t err = adc_continuous_read(s_adc, raw_buf, sizeof(raw_buf),
                                        &out_len, pdMS_TO_TICKS(100));
    if (err != ESP_OK || out_len == 0) {
        ESP_LOGW(TAG, "ADC read failed: %s", esp_err_to_name(err));
        return false;
    }

    int n = (int)(out_len / sizeof(adc_digi_output_data_t));
    if (n > BIA_FRAME_SAMPLES) {
        n = BIA_FRAME_SAMPLES;
    }

    // Convert to float and remove DC (virtual GND ~2.5 V appears as ~3100 counts).
    float mean = 0.0f;
    for (int i = 0; i < n; i++) {
        adc_digi_output_data_t *p =
            (adc_digi_output_data_t *)(raw_buf + i * sizeof(adc_digi_output_data_t));
        samples[i] = (float)p->type1.data;
        mean += samples[i];
    }
    mean /= (float)n;
    for (int i = 0; i < n; i++) {
        samples[i] -= mean;
    }

    float amp_counts = goertzel_amplitude(samples, n,
                                          (float)BIA_SAMPLE_RATE_HZ,
                                          (float)BIA_FREQ_HZ);

    float v_peak_at_adc  = amp_counts * ADC_VREF_V / ADC_FULL_SCALE;
    float v_rms_at_adc   = v_peak_at_adc / sqrtf(2.0f);
    float v_rms_at_body  = v_rms_at_adc / AD620_GAIN;

    *z_ohms = v_rms_at_body / INJECTION_CURRENT_RMS_A;
    return true;
}

static int cmp_float(const void *a, const void *b)
{
    float fa = *(const float *)a, fb = *(const float *)b;
    return (fa > fb) - (fa < fb);
}

bool bia_measure_impedance_avg(float *z_ohms)
{
    enum { N_READS = 20, TRIM = 5 };
    float buf[N_READS];
    int n = 0;

    for (int i = 0; i < N_READS; i++) {
        float z;
        if (bia_measure_impedance(&z)) {
            buf[n++] = z;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    if (n < N_READS - 4) {
        return false;
    }

    qsort(buf, n, sizeof(float), cmp_float);

    int lo = TRIM, hi = n - TRIM;
    if (hi <= lo) {
        return false;
    }

    float sum = 0.0f;
    for (int i = lo; i < hi; i++) {
        sum += buf[i];
    }
    *z_ohms = sum / (float)(hi - lo);
    return true;
}

float bia_estimate_body_fat_pct(float z_ohms, float height_cm, float weight_kg,
                                int age, bia_sex_t sex)
{
    // Deurenberg single-frequency BIA equation:
    //   FFM = -12.44 + 0.34*(H^2/Z) + 0.1534*H + 0.273*W - 0.127*A + 4.56*S
    //   H in cm, W in kg, A in years, S = 1 male / 0 female
    float h2_over_z = (height_cm * height_cm) / z_ohms;
    float ffm = -12.44f
              + 0.34f   * h2_over_z
              + 0.1534f * height_cm
              + 0.273f  * weight_kg
              - 0.127f  * (float)age
              + 4.56f   * (float)sex;

    if (ffm < 0.0f)        ffm = 0.0f;
    if (ffm > weight_kg)   ffm = weight_kg;

    return (weight_kg - ffm) / weight_kg * 100.0f;
}
