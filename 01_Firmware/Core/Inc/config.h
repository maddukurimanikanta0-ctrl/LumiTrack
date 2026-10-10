/**
  ******************************************************************************
  * @file    config.h
  * @brief   System configuration, pin definitions, and constants for LumiTrack
  *          STM32 NUCLEO-F446RE (Parent Problem 10 / Variant 10.03)
  ******************************************************************************
  */

#ifndef __CONFIG_H
#define __CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ========================================================================== */
/* Project & Variant Metadata                                                 */
/* ========================================================================== */
#define PROJECT_NAME                "LumiTrack-F446RE"
#define FIRMWARE_VERSION            "1.0.0"
#define VARIANT_ID                  "10.03"
#define VARIANT_NAME                "Museum Exposure Limiter"

/* ========================================================================== */
/* Hardware Pin Definitions for STM32 NUCLEO-F446RE                           */
/* ========================================================================== */
/* Status LED (Onboard Green LED LD2) */
#define LD2_GPIO_PORT               GPIOA
#define LD2_PIN                     GPIO_PIN_5

/* User Push-Button (Onboard Blue Button B1) */
#define B1_GPIO_PORT                GPIOC
#define B1_PIN                     GPIO_PIN_13

/* LDR Analog Input (Arduino Header A0) -> ADC1_IN0 */
#define LDR_ADC_CHANNEL             ADC_CHANNEL_0
#define LDR_GPIO_PORT               GPIOA
#define LDR_PIN                     GPIO_PIN_0

/* Potentiometer Analog Input (Arduino Header A1) -> ADC1_IN1 */
#define POT_ADC_CHANNEL             ADC_CHANNEL_1
#define POT_GPIO_PORT               GPIOA
#define POT_PIN                     GPIO_PIN_1

/* Servo PWM Output -> TIM3_CH1 on PA6 (Arduino Header D12) */
#define SERVO_TIM_INSTANCE          TIM3
#define SERVO_TIM_CHANNEL           TIM_CHANNEL_1
#define SERVO_GPIO_PORT             GPIOA
#define SERVO_PIN                   GPIO_PIN_6
#define SERVO_GPIO_AF               GPIO_AF2_TIM3

/* Virtual COM Port UART (ST-LINK USART2) */
/* USART2_TX -> PA2, USART2_RX -> PA3 */
#define VCP_UART_INSTANCE           USART2
#define VCP_UART_BAUDRATE           115200
#define VCP_GPIO_PORT               GPIOA
#define VCP_TX_PIN                  GPIO_PIN_2
#define VCP_RX_PIN                  GPIO_PIN_3
#define VCP_GPIO_AF                 GPIO_AF7_USART2

/* Optional I2C Display (SSD1306 OLED / LCD) on I2C1 */
/* SCL -> PB8 (D15), SDA -> PB9 (D14) */
#define I2C_INSTANCE                I2C1
#define I2C_GPIO_PORT               GPIOB
#define I2C_SCL_PIN                 GPIO_PIN_8
#define I2C_SDA_PIN                 GPIO_PIN_9
#define I2C_GPIO_AF                 GPIO_AF4_I2C1

/* ========================================================================== */
/* Approved Servo Angle Grid                                                  */
/* Strictly discrete approved angle grid: 7 marked positions                  */
/* ========================================================================== */
#define NUM_APPROVED_ANGLES         7

/* Approved Angle Values in Degrees */
/* POS_0: 15°, POS_1: 40°, POS_2: 65°, POS_3: 90° (SAFE_HOME),               */
/* POS_4: 115°, POS_5: 140°, POS_6: 165°                                      */
static const uint16_t APPROVED_ANGLES_DEG[NUM_APPROVED_ANGLES] = {
    15,     /* POS_0: Far Left Tilt */
    40,     /* POS_1: Medium Left */
    65,     /* POS_2: Shallow Left */
    90,     /* POS_3: Neutral / Safe Home / Center */
    115,    /* POS_4: Shallow Right */
    140,    /* POS_5: Medium Right */
    165     /* POS_6: Far Right Tilt */
};

#define SAFE_HOME_ANGLE_ID          3       /* Default safe neutral angle (90°) */
#define SAFE_HOME_ANGLE_DEG         90

/* ========================================================================== */
/* Timer & PWM Timings (50 Hz Servo PWM: 20ms period)                         */
/* Timer clock: 90 MHz (APB1 Timer). Prescaler = 90 - 1 -> 1 MHz (1 tick = 1us)*/
/* Auto-Reload ARR = 20000 - 1 (20,000 us = 20 ms)                            */
/* 0° = 1000 us (CCR = 1000), 180° = 2000 us (CCR = 2000)                    */
/* ========================================================================== */
#define SERVO_TIMER_PRESCALER       (90 - 1)
#define SERVO_TIMER_PERIOD          (20000 - 1)
#define SERVO_MIN_PULSE_US          1000    /* 1.0 ms pulse for 0 degrees */
#define SERVO_MAX_PULSE_US          2000    /* 2.0 ms pulse for 180 degrees */
#define SERVO_SETTLING_TIME_MS      150     /* Mechanical settling time per stop */
#define SERVO_SLEW_STEP_MS          20      /* Slew rate interpolation step delay */

/* ========================================================================== */
/* Safety Watchdog & Limits                                                   */
/* ========================================================================== */
#define HOST_WATCHDOG_TIMEOUT_MS    3000    /* Failsafe timeout: 3 sec without ping reverts to home */
#define TELEMETRY_INTERVAL_MS       200     /* Periodic telemetry frame interval */
#define ADC_OVERSAMPLE_COUNT        16      /* Number of ADC samples averaged */
#define MANUAL_OVERRIDE_DEADBAND    100     /* Potentiometer ADC movement delta to trigger manual */

/* Museum Conservation Cumulative Exposure Target (normalized lux-seconds)   */
#define CUMULATIVE_LUX_TARGET       50000.0f

#ifdef __cplusplus
}
#endif

#endif /* __CONFIG_H */
