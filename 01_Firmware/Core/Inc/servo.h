/**
  ******************************************************************************
  * @file    servo.h
  * @brief   Servo motor PWM driver with approved angle grid mapping
  ******************************************************************************
  */

#ifndef __SERVO_H
#define __SERVO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include "config.h"
#include <stdbool.h>

void Servo_Init(TIM_HandleTypeDef *htim, uint32_t channel);
bool Servo_MoveToAngleId(uint8_t angle_id);
bool Servo_MoveToAngleDeg(uint16_t target_deg);
void Servo_MoveToSafeHome(void);

uint8_t Servo_GetCurrentAngleId(void);
uint16_t Servo_GetCurrentAngleDeg(void);
uint16_t Servo_GetTargetAngleDeg(void);
bool Servo_IsSettled(void);
void Servo_UpdateSlew(uint32_t current_time_ms);

#ifdef __cplusplus
}
#endif

#endif /* __SERVO_H */
