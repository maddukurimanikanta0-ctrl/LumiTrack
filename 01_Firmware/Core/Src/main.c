/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body for LumiTrack - Intelligent Light
  *                   Mapping and Positioning System (Variant 10.03)
  *                   Target Microcontroller: STM32F446RET6 (NUCLEO-F446RE)
  ******************************************************************************
  */

#include "main.h"
#include "config.h"
#include "safety.h"
#include "servo.h"
#include "sensors.h"
#include "protocol.h"
#include <stdbool.h>
#include <string.h>

/* Global Peripheral Handles */
ADC_HandleTypeDef hadc1;
TIM_HandleTypeDef htim3;
UART_HandleTypeDef huart2;

/* Private Function Prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART2_UART_Init(void);
static void ExecuteScanProfile(ScanProfile_t profile);

/* Main Application Loop */
int main(void) {
    /* Reset of all peripherals, Initializes the Flash interface and the Systick */
    HAL_Init();

    /* Configure the system clock to 180 MHz */
    SystemClock_Config();

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_TIM3_Init();
    MX_USART2_UART_Init();

    /* Initialize subsystems */
    Safety_Init();
    Servo_Init(&htim3, SERVO_TIM_CHANNEL);
    Sensors_Init(&hadc1);
    Protocol_Init(&huart2);

    /* Move to safe home at startup */
    Servo_MoveToSafeHome();
    Safety_SetState(STATE_SAFE_HOME);

    uint32_t last_telem_tick = 0;
    uint32_t last_sensor_tick = 0;
    uint32_t last_led_tick = 0;
    uint32_t telem_seq = 0;
    uint32_t run_id = 1;

    uint8_t rx_byte = 0;

    /* Infinite loop */
    while (1) {
        uint32_t now = HAL_GetTick();

        /* ------------------------------------------------------------------ */
        /* 1. Non-blocking UART Reception with Error Recovery                 */
        /* ------------------------------------------------------------------ */
        if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE)) {
            __HAL_UART_CLEAR_OREFLAG(&huart2);
        }
        while (HAL_UART_Receive(&huart2, &rx_byte, 1, 0) == HAL_OK) {
            Protocol_ReceiveByte(rx_byte);
        }

        /* ------------------------------------------------------------------ */
        /* 2. Process Received Host Commands                                  */
        /* ------------------------------------------------------------------ */
        if (Protocol_HasPendingCommand()) {
            HostCommand_t cmd;
            if (Protocol_GetCommand(&cmd)) {
                Safety_FeedWatchdog();

                /* Check if manual override is currently active */
                if (Safety_IsManualOverrideActive() && cmd.type != CMD_TYPE_RETURN_HOME) {
                    Protocol_SendNack(&cmd, REASON_ERR_MANUAL_OVERRIDE_ACTIVE, Servo_GetCurrentAngleId());
                } else {
                    switch (cmd.type) {
                        case CMD_TYPE_ANGLE_ID:
                            if (!Safety_ValidateAngleId(cmd.angle_id)) {
                                Protocol_SendNack(&cmd, REASON_ERR_OUT_OF_BOUNDS, Servo_GetCurrentAngleId());
                            } else if (!Safety_ValidateCommandExpiry(cmd.expiry_ms, cmd.received_tick, now)) {
                                Protocol_SendNack(&cmd, REASON_ERR_EXPIRED, Servo_GetCurrentAngleId());
                            } else {
                                Servo_MoveToAngleId(cmd.angle_id);
                                Safety_SetState(STATE_POSITIONED);
                                Protocol_SendAck(&cmd, REASON_OK, cmd.angle_id, APPROVED_ANGLES_DEG[cmd.angle_id]);
                            }
                            break;

                        case CMD_TYPE_SCAN_PROFILE:
                            Protocol_SendAck(&cmd, REASON_OK, Servo_GetCurrentAngleId(), Servo_GetCurrentAngleDeg());
                            ExecuteScanProfile(cmd.profile);
                            break;

                        case CMD_TYPE_HOLD:
                            Protocol_SendAck(&cmd, REASON_OK, Servo_GetCurrentAngleId(), Servo_GetCurrentAngleDeg());
                            break;

                        case CMD_TYPE_RETURN_HOME:
                            Servo_MoveToSafeHome();
                            Safety_SetState(STATE_SAFE_HOME);
                            Safety_SetManualOverride(false);
                            Protocol_SendAck(&cmd, REASON_OK, SAFE_HOME_ANGLE_ID, SAFE_HOME_ANGLE_DEG);
                            break;

                        default:
                            Protocol_SendNack(&cmd, REASON_ERR_INVALID_COMMAND, Servo_GetCurrentAngleId());
                            break;
                    }
                }
            }
        }

        /* ------------------------------------------------------------------ */
        /* 3. Sensor Acquisition & Exposure Integration (Every 50 ms)         */
        /* ------------------------------------------------------------------ */
        if ((now - last_sensor_tick) >= 50) {
            float dt_sec = (float)(now - last_sensor_tick) / 1000.0f;
            last_sensor_tick = now;

            Sensors_Sample();
            Sensors_UpdateCumulativeExposure(dt_sec);

            /* Check Potentiometer for Manual Override */
            if (Sensors_CheckManualOverrideTrigger()) {
                Safety_SetManualOverride(true);
                /* Map potentiometer directly to nearest approved angle */
                const SensorData_t *s = Sensors_GetData();
                uint8_t target_id = (uint8_t)(((uint32_t)s->pot_raw * (NUM_APPROVED_ANGLES - 1)) / 4095);
                Servo_MoveToAngleId(target_id);
            }

            /* Check Blue Button B1 for Override Release / Scan Trigger */
            if (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET) {
                HAL_Delay(50); /* Debounce */
                if (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET) {
                    if (Safety_IsManualOverrideActive()) {
                        Safety_SetManualOverride(false);
                        Servo_MoveToSafeHome();
                    } else {
                        /* Trigger a local scan */
                        ExecuteScanProfile(SCAN_PROFILE_FULL);
                        run_id++;
                    }
                }
            }
        }

        /* ------------------------------------------------------------------ */
        /* 4. Safety State Machine & Watchdog Check                           */
        /* ------------------------------------------------------------------ */
        Safety_Update(now);
        if (Safety_GetState() == STATE_FAULT_HOST_TIMEOUT) {
            /* Failsafe triggered: Move servo to safe neutral home position */
            Servo_MoveToSafeHome();
        }

        /* ------------------------------------------------------------------ */
        /* 5. Smooth Servo Slew Rate Update                                   */
        /* ------------------------------------------------------------------ */
        Servo_UpdateSlew(now);

        /* ------------------------------------------------------------------ */
        /* 6. Periodic Telemetry Stream (Every 200 ms)                         */
        /* ------------------------------------------------------------------ */
        if ((now - last_telem_tick) >= TELEMETRY_INTERVAL_MS) {
            last_telem_tick = now;
            telem_seq++;
            Protocol_SendTelemetry(run_id, telem_seq,
                                   Servo_GetCurrentAngleId(),
                                   Servo_GetCurrentAngleDeg(),
                                   Sensors_GetData(),
                                   Safety_GetState());
        }

        /* ------------------------------------------------------------------ */
        /* 7. Heartbeat LED Indication                                        */
        /* ------------------------------------------------------------------ */
        uint32_t led_period = 500; /* Normal: 2 Hz toggle */
        if (Safety_GetState() == STATE_FAULT_HOST_TIMEOUT) {
            led_period = 100; /* Fast 10 Hz blink on timeout alert */
        } else if (Safety_GetState() == STATE_SCANNING) {
            led_period = 250; /* 4 Hz blink during scan */
        }

        if ((now - last_led_tick) >= led_period) {
            last_led_tick = now;
            HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
        }
    }
}

/**
  * @brief  Executes a structured angular scan profile across approved positions
  */
static void ExecuteScanProfile(ScanProfile_t profile) {
    Safety_SetState(STATE_SCANNING);

    uint8_t start_idx = 0;
    uint8_t end_idx = NUM_APPROVED_ANGLES;
    uint8_t step = 1;

    if (profile == SCAN_PROFILE_PARTIAL) {
        /* Sweep first 4 points (POS_0 to POS_3) for early ML confidence stopping */
        end_idx = 4;
    } else if (profile == SCAN_PROFILE_FAST) {
        /* Sweep every alternate point (0, 2, 4, 6) */
        step = 2;
    }

    uint8_t point_count = 0;
    uint16_t angles_recorded[NUM_APPROVED_ANGLES];
    float lux_recorded[NUM_APPROVED_ANGLES];

    for (uint8_t i = start_idx; i < end_idx; i += step) {
        Servo_MoveToAngleId(i);

        /* Wait for servo slew and mechanical settling while servicing UART */
        while (!Servo_IsSettled()) {
            Servo_UpdateSlew(HAL_GetTick());
            uint8_t b;
            if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE)) {
                __HAL_UART_CLEAR_OREFLAG(&huart2);
            }
            while (HAL_UART_Receive(&huart2, &b, 1, 0) == HAL_OK) {
                Protocol_ReceiveByte(b);
            }
            HAL_Delay(5);
        }

        /* Sample sensors at settled position */
        Sensors_Sample();
        const SensorData_t *data = Sensors_GetData();

        angles_recorded[point_count] = APPROVED_ANGLES_DEG[i];
        lux_recorded[point_count] = data->lux_estimate;
        point_count++;

        /* Send instantaneous telem for this step */
        Protocol_SendTelemetry(1, point_count, i, APPROVED_ANGLES_DEG[i], data, STATE_SCANNING);
    }

    /* Emit complete scan payload */
    Protocol_SendScanComplete(profile, point_count, angles_recorded, lux_recorded);

    /* Return to safe home after scan completion */
    Servo_MoveToSafeHome();
    Safety_SetState(STATE_IDLE);
}

/**
  * @brief System Clock Configuration (180 MHz SYSCLK for STM32F446RE)
  */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* Use HSI (16 MHz) as PLL source for robust standalone clocking */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 16;
    RCC_OscInitStruct.PLL.PLLN = 360;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    RCC_OscInitStruct.PLL.PLLR = 2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /* Activate Over-Drive mode for 180 MHz operation */
    if (HAL_PWREx_EnableOverDrive() != HAL_OK) {
        Error_Handler();
    }

    /* Initialize CPU, AHB and APB bus clocks */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }
}

/**
  * @brief ADC1 Initialization Function (LDR on PA0, Potentiometer on PA1)
  */
static void MX_ADC1_Init(void) {
    ADC_ChannelConfTypeDef sConfig = {0};

    __HAL_RCC_ADC1_CLK_ENABLE();

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        Error_Handler();
    }

    /* Configure initial channel PA0 (LDR) */
    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }
}

/**
  * @brief TIM3 Initialization Function (50 Hz PWM for Servo on PA6)
  */
static void MX_TIM3_Init(void) {
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_OC_InitTypeDef sConfigOC = {0};

    __HAL_RCC_TIM3_CLK_ENABLE();

    /* APB1 Timer Clock = 90 MHz. Prescaler = 90 - 1 = 89 -> 1 MHz clock (1 us/tick) */
    /* Period = 20000 - 1 = 19999 -> 20 ms period (50 Hz) */
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = SERVO_TIMER_PRESCALER;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = SERVO_TIMER_PERIOD;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) {
        Error_Handler();
    }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK) {
        Error_Handler();
    }

    /* Configure PWM Channel 1 for PA6 */
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 1500; /* Initial neutral pulse 1.5ms (90 deg) */
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) {
        Error_Handler();
    }
}

/**
  * @brief USART2 Initialization Function (VCP on PA2/PA3 @ 115200 baud)
  */
static void MX_USART2_UART_Init(void) {
    __HAL_RCC_USART2_CLK_ENABLE();

    huart2.Instance = USART2;
    huart2.Init.BaudRate = VCP_UART_BAUDRATE;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) {
        Error_Handler();
    }
}

/**
  * @brief GPIO Initialization Function
  */
static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Configure LD2 Pin (PA5) as Output */
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin = LD2_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

    /* Configure B1 Pin (PC13) as Input with Pull-up */
    GPIO_InitStruct.Pin = B1_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

    /* Configure ADC Pins: PA0 (LDR), PA1 (Pot) as Analog */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* Configure PA6 as TIM3_CH1 Alternate Function */
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* Configure PA2 (TX) and PA3 (RX) as USART2 Alternate Function */
    GPIO_InitStruct.Pin = USART_TX_Pin | USART_RX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/**
  * @brief  This function is executed in case of error occurrence.
  */
void Error_Handler(void) {
    __disable_irq();
    while (1) {
        HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
        for (volatile int i = 0; i < 200000; i++);
    }
}
