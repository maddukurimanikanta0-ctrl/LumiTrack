/**
  ******************************************************************************
  * @file    sensors.h
  * @brief   ADC sensor acquisition, LDR light conversion, and cumulative exposure
  ******************************************************************************
  */

#ifndef __SENSORS_H
#define __SENSORS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include "config.h"
#include <stdbool.h>

typedef struct {
    uint16_t ldr_raw;           /* Raw 12-bit ADC value (0-4095) */
    float    ldr_voltage;       /* Measured voltage (0.0 - 3.3V) */
    float    lux_estimate;      /* Calibrated light intensity (lux) */
    uint16_t pot_raw;           /* Potentiometer ADC reading (0-4095) */
    float    pot_voltage;       /* Potentiometer voltage (0.0 - 3.3V) */
    float    target_threshold;  /* Target exposure limit set by pot */
    float    cumulative_exposure;/* Accumulated light exposure (lux-seconds) */
    uint16_t dark_offset;       /* Dark reference calibration offset */
} SensorData_t;

void Sensors_Init(ADC_HandleTypeDef *hadc);
void Sensors_Sample(void);
const SensorData_t* Sensors_GetData(void);

void Sensors_CalibrateDark(void);
void Sensors_ResetCumulativeExposure(void);
void Sensors_UpdateCumulativeExposure(float dt_seconds);

bool Sensors_CheckManualOverrideTrigger(void);

#ifdef __cplusplus
}
#endif

#endif /* __SENSORS_H */
