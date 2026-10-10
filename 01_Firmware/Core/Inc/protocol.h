/**
  ******************************************************************************
  * @file    protocol.h
  * @brief   Canonical command protocol parser, telemetry encoder, and ACK/NACK
  *          Strict adherence to Parent 10 command contract:
  *          ANGLE_ID | SCAN_PROFILE | HOLD | RETURN_HOME
  ******************************************************************************
  */

#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include "config.h"
#include "safety.h"
#include "sensors.h"
#include <stdbool.h>

typedef enum {
    CMD_TYPE_NONE = 0,
    CMD_TYPE_ANGLE_ID,
    CMD_TYPE_SCAN_PROFILE,
    CMD_TYPE_HOLD,
    CMD_TYPE_RETURN_HOME
} CommandType_t;

typedef enum {
    SCAN_PROFILE_FULL = 0,
    SCAN_PROFILE_FAST,
    SCAN_PROFILE_PARTIAL
} ScanProfile_t;

typedef struct {
    CommandType_t type;
    uint8_t       angle_id;
    ScanProfile_t profile;
    uint32_t      seq_num;
    char          model_version[16];
    float         confidence;
    uint32_t      expiry_ms;
    uint32_t      received_tick;
} HostCommand_t;

void Protocol_Init(UART_HandleTypeDef *huart);
void Protocol_ReceiveByte(uint8_t rx_char);
bool Protocol_HasPendingCommand(void);
bool Protocol_GetCommand(HostCommand_t *cmd);

void Protocol_SendAck(const HostCommand_t *cmd, AckReason_t reason, uint8_t current_angle_id, uint16_t current_angle_deg);
void Protocol_SendNack(const HostCommand_t *cmd, AckReason_t reason, uint8_t current_angle_id);
void Protocol_SendTelemetry(uint32_t run_id, uint32_t seq, uint8_t angle_id, uint16_t angle_deg, const SensorData_t *sensors, SystemState_t state);
void Protocol_SendScanComplete(ScanProfile_t profile, uint8_t num_points, const uint16_t *angles_deg, const float *lux_values);

#ifdef __cplusplus
}
#endif

#endif /* __PROTOCOL_H */
