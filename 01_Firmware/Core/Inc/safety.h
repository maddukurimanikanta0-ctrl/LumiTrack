/**
  ******************************************************************************
  * @file    safety.h
  * @brief   Safety subsystem, state machine, and hard limits for LumiTrack
  ******************************************************************************
  */

#ifndef __SAFETY_H
#define __SAFETY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"
#include <stdbool.h>

typedef enum {
    STATE_INIT = 0,
    STATE_SAFE_HOME,
    STATE_IDLE,
    STATE_SCANNING,
    STATE_POSITIONED,
    STATE_MANUAL_OVERRIDE,
    STATE_FAULT_HOST_TIMEOUT
} SystemState_t;

typedef enum {
    REASON_OK = 0,
    REASON_ERR_OUT_OF_BOUNDS,
    REASON_ERR_EXPIRED,
    REASON_ERR_MANUAL_OVERRIDE_ACTIVE,
    REASON_ERR_INVALID_COMMAND,
    REASON_ERR_CHECKSUM,
    REASON_ERR_BUSY
} AckReason_t;

void Safety_Init(void);
void Safety_Update(uint32_t current_time_ms);
void Safety_FeedWatchdog(void);
bool Safety_IsHostTimedOut(void);

bool Safety_ValidateAngleId(uint8_t angle_id);
bool Safety_ValidateCommandExpiry(uint32_t cmd_expiry_ms, uint32_t received_tick_ms, uint32_t current_time_ms);

void Safety_SetState(SystemState_t new_state);
SystemState_t Safety_GetState(void);
const char* Safety_GetStateString(SystemState_t state);
const char* Safety_GetReasonString(AckReason_t reason);

bool Safety_IsManualOverrideActive(void);
void Safety_SetManualOverride(bool active);

#ifdef __cplusplus
}
#endif

#endif /* __SAFETY_H */
