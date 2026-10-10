/**
  ******************************************************************************
  * @file    servo.c
  * @brief   Servo motor PWM driver with approved angle grid mapping & slew rate
  ******************************************************************************
  */

#include "servo.h"

static TIM_HandleTypeDef *s_htim = NULL;
static uint32_t s_channel = 0;

static uint8_t s_current_angle_id = SAFE_HOME_ANGLE_ID;
static uint16_t s_current_angle_deg = SAFE_HOME_ANGLE_DEG;
static uint16_t s_target_angle_deg = SAFE_HOME_ANGLE_DEG;

static uint32_t s_last_step_time_ms = 0;
static uint32_t s_target_reach_time_ms = 0;

static uint32_t AngleToPulseUs(uint16_t angle_deg) {
    if (angle_deg > 180) {
        angle_deg = 180;
    }
    /* Linear interpolation between SERVO_MIN_PULSE_US (0 deg) and SERVO_MAX_PULSE_US (180 deg) */
    uint32_t pulse = SERVO_MIN_PULSE_US + ((uint32_t)angle_deg * (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US)) / 180;
    return pulse;
}

static void SetPwmPulse(uint32_t pulse_us) {
    if (s_htim != NULL) {
        __HAL_TIM_SET_COMPARE(s_htim, s_channel, pulse_us);
    }
}

void Servo_Init(TIM_HandleTypeDef *htim, uint32_t channel) {
    s_htim = htim;
    s_channel = channel;
    s_current_angle_id = SAFE_HOME_ANGLE_ID;
    s_current_angle_deg = SAFE_HOME_ANGLE_DEG;
    s_target_angle_deg = SAFE_HOME_ANGLE_DEG;
    s_last_step_time_ms = 0;

    /* Start PWM output */
    HAL_TIM_PWM_Start(s_htim, s_channel);
    SetPwmPulse(AngleToPulseUs(s_current_angle_deg));
}

bool Servo_MoveToAngleId(uint8_t angle_id) {
    if (angle_id >= NUM_APPROVED_ANGLES) {
        return false;
    }
    s_current_angle_id = angle_id;
    s_target_angle_deg = APPROVED_ANGLES_DEG[angle_id];
    return true;
}

bool Servo_MoveToAngleDeg(uint16_t target_deg) {
    /* Check if target matches any approved angle */
    for (uint8_t i = 0; i < NUM_APPROVED_ANGLES; i++) {
        if (APPROVED_ANGLES_DEG[i] == target_deg) {
            s_current_angle_id = i;
            s_target_angle_deg = target_deg;
            return true;
        }
    }
    return false; /* Disallow arbitrary unapproved angles */
}

void Servo_MoveToSafeHome(void) {
    s_current_angle_id = SAFE_HOME_ANGLE_ID;
    s_target_angle_deg = SAFE_HOME_ANGLE_DEG;
}

uint8_t Servo_GetCurrentAngleId(void) {
    return s_current_angle_id;
}

uint16_t Servo_GetCurrentAngleDeg(void) {
    return s_current_angle_deg;
}

uint16_t Servo_GetTargetAngleDeg(void) {
    return s_target_angle_deg;
}

bool Servo_IsSettled(void) {
    extern uint32_t HAL_GetTick(void);
    uint32_t now = HAL_GetTick();
    if (s_current_angle_deg == s_target_angle_deg) {
        if ((now - s_target_reach_time_ms) >= SERVO_SETTLING_TIME_MS) {
            return true;
        }
    }
    return false;
}

void Servo_UpdateSlew(uint32_t current_time_ms) {
    if ((current_time_ms - s_last_step_time_ms) < SERVO_SLEW_STEP_MS) {
        return;
    }
    s_last_step_time_ms = current_time_ms;

    if (s_current_angle_deg < s_target_angle_deg) {
        s_current_angle_deg += 1;
        SetPwmPulse(AngleToPulseUs(s_current_angle_deg));
        if (s_current_angle_deg == s_target_angle_deg) {
            s_target_reach_time_ms = current_time_ms;
        }
    } else if (s_current_angle_deg > s_target_angle_deg) {
        s_current_angle_deg -= 1;
        SetPwmPulse(AngleToPulseUs(s_current_angle_deg));
        if (s_current_angle_deg == s_target_angle_deg) {
            s_target_reach_time_ms = current_time_ms;
        }
    } else {
        /* Already at target */
    }
}
