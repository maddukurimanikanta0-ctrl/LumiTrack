/**
  ******************************************************************************
  * @file    sensors.c
  * @brief   ADC acquisition, oversampling, LDR lux conversion & cumulative meter
  ******************************************************************************
  */

#include "sensors.h"
#include <math.h>
#include <stdlib.h>

static ADC_HandleTypeDef *s_hadc = NULL;
static SensorData_t s_sensor_data;
static uint16_t s_last_pot_raw = 0;
static bool s_pot_baseline_initialized = false;

static uint32_t ReadAdcChannel(uint32_t channel) {
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = channel;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;

    HAL_ADC_ConfigChannel(s_hadc, &sConfig);

    uint32_t sum = 0;
    for (int i = 0; i < ADC_OVERSAMPLE_COUNT; i++) {
        HAL_ADC_Start(s_hadc);
        if (HAL_ADC_PollForConversion(s_hadc, 10) == HAL_OK) {
            sum += HAL_ADC_GetValue(s_hadc);
        }
        HAL_ADC_Stop(s_hadc);
    }
    return (sum / ADC_OVERSAMPLE_COUNT);
}

void Sensors_Init(ADC_HandleTypeDef *hadc) {
    s_hadc = hadc;
    s_sensor_data.ldr_raw = 0;
    s_sensor_data.ldr_voltage = 0.0f;
    s_sensor_data.lux_estimate = 0.0f;
    s_sensor_data.pot_raw = 2048;
    s_sensor_data.pot_voltage = 1.65f;
    s_sensor_data.target_threshold = 500.0f;
    s_sensor_data.cumulative_exposure = 0.0f;
    s_sensor_data.dark_offset = 0;
    s_last_pot_raw = 2048;
    s_pot_baseline_initialized = false;
}

void Sensors_Sample(void) {
    if (s_hadc == NULL) return;

    /* Read LDR on ADC Channel 0 */
    uint32_t raw_ldr = ReadAdcChannel(LDR_ADC_CHANNEL);
    if (raw_ldr > s_sensor_data.dark_offset) {
        s_sensor_data.ldr_raw = (uint16_t)(raw_ldr - s_sensor_data.dark_offset);
    } else {
        s_sensor_data.ldr_raw = 0;
    }

    s_sensor_data.ldr_voltage = ((float)s_sensor_data.ldr_raw * 3.3f) / 4095.0f;

    /* Approximate Lux Conversion: standard exponential/power law for typical GL5528 LDR */
    /* Lux ~ 1000 * (V / (3.3 - V))^1.4 */
    if (s_sensor_data.ldr_voltage < 0.01f) {
        s_sensor_data.lux_estimate = 1.0f;
    } else if (s_sensor_data.ldr_voltage > 3.25f) {
        s_sensor_data.lux_estimate = 2000.0f;
    } else {
        float r_ratio = s_sensor_data.ldr_voltage / (3.3f - s_sensor_data.ldr_voltage + 0.001f);
        if (r_ratio < 0.0f) r_ratio = 0.0f;
        s_sensor_data.lux_estimate = 500.0f * powf(r_ratio, 1.25f);
    }

    /* Read Potentiometer on ADC Channel 1 */
    uint32_t raw_pot = ReadAdcChannel(POT_ADC_CHANNEL);
    s_sensor_data.pot_raw = (uint16_t)raw_pot;
    s_sensor_data.pot_voltage = ((float)s_sensor_data.pot_raw * 3.3f) / 4095.0f;

    /* Map Potentiometer (0..4095) to target lux threshold (50 lux to 1500 lux) */
    s_sensor_data.target_threshold = 50.0f + ((float)s_sensor_data.pot_raw * 1450.0f) / 4095.0f;

    if (!s_pot_baseline_initialized) {
        s_last_pot_raw = s_sensor_data.pot_raw;
        s_pot_baseline_initialized = true;
    }
}

const SensorData_t* Sensors_GetData(void) {
    return &s_sensor_data;
}

void Sensors_CalibrateDark(void) {
    /* Read current LDR level as ambient dark baseline tare */
    uint32_t raw = ReadAdcChannel(LDR_ADC_CHANNEL);
    s_sensor_data.dark_offset = (uint16_t)raw;
}

void Sensors_ResetCumulativeExposure(void) {
    s_sensor_data.cumulative_exposure = 0.0f;
}

void Sensors_UpdateCumulativeExposure(float dt_seconds) {
    s_sensor_data.cumulative_exposure += (s_sensor_data.lux_estimate * dt_seconds);
}

bool Sensors_CheckManualOverrideTrigger(void) {
    if (!s_pot_baseline_initialized) {
        return false;
    }
    int diff = abs((int)s_sensor_data.pot_raw - (int)s_last_pot_raw);
    if (diff > MANUAL_OVERRIDE_DEADBAND) {
        s_last_pot_raw = s_sensor_data.pot_raw;
        return true;
    }
    return false;
}
