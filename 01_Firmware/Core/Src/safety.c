/**
  ******************************************************************************
  * @file    safety.c
  * @brief   Implementation of safety subsystem and state machine
  ******************************************************************************
  */

#include "safety.h"
#include <string.h>

static SystemState_t current_state = STATE_INIT;
static uint32_t last_host_ping_ms = 0;
static bool manual_override_flag = false;

void Safety_Init(void) {
    current_state = STATE_INIT;
    last_host_ping_ms = 0;
    manual_override_flag = false;
}

void Safety_FeedWatchdog(void) {
    /* Called whenever a valid host packet is received */
    extern uint32_t HAL_GetTick(void);
    last_host_ping_ms = HAL_GetTick();

    if (current_state == STATE_FAULT_HOST_TIMEOUT) {
        current_state = STATE_IDLE;
    }
}

bool Safety_IsHostTimedOut(void) {
    extern uint32_t HAL_GetTick(void);
    uint32_t now = HAL_GetTick();

    if (last_host_ping_ms == 0) {
        return false; /* Host hasn't connected yet initially */
    }

    if ((now - last_host_ping_ms) > HOST_WATCHDOG_TIMEOUT_MS) {
        return true;
    }
    return false;
}

void Safety_Update(uint32_t current_time_ms) {
    if (manual_override_flag) {
        current_state = STATE_MANUAL_OVERRIDE;
        return;
    }

    if (Safety_IsHostTimedOut()) {
        current_state = STATE_FAULT_HOST_TIMEOUT;
    }
}

bool Safety_ValidateAngleId(uint8_t angle_id) {
    /* Strict clamping check against approved discrete angle grid */
    if (angle_id < NUM_APPROVED_ANGLES) {
        return true;
    }
    return false;
}

bool Safety_ValidateCommandExpiry(uint32_t cmd_expiry_ms, uint32_t received_tick_ms, uint32_t current_time_ms) {
    /* If expiry_ms is 0, command has no expiration */
    if (cmd_expiry_ms == 0) {
        return true;
    }
    /* Verify command was processed within its allowable execution window */
    if ((current_time_ms - received_tick_ms) <= cmd_expiry_ms) {
        return true;
    }
    return false;
}

void Safety_SetState(SystemState_t new_state) {
    current_state = new_state;
}

SystemState_t Safety_GetState(void) {
    return current_state;
}

const char* Safety_GetStateString(SystemState_t state) {
    switch (state) {
        case STATE_INIT:                return "INIT";
        case STATE_SAFE_HOME:           return "SAFE_HOME";
        case STATE_IDLE:                return "IDLE";
        case STATE_SCANNING:            return "SCANNING";
        case STATE_POSITIONED:          return "POSITIONED";
        case STATE_MANUAL_OVERRIDE:     return "MANUAL_OVERRIDE";
        case STATE_FAULT_HOST_TIMEOUT:  return "FAULT_HOST_TIMEOUT";
        default:                        return "UNKNOWN";
    }
}

const char* Safety_GetReasonString(AckReason_t reason) {
    switch (reason) {
        case REASON_OK:                         return "OK";
        case REASON_ERR_OUT_OF_BOUNDS:          return "ERR_OUT_OF_BOUNDS";
        case REASON_ERR_EXPIRED:                return "ERR_EXPIRED";
        case REASON_ERR_MANUAL_OVERRIDE_ACTIVE: return "ERR_MANUAL_OVERRIDE_ACTIVE";
        case REASON_ERR_INVALID_COMMAND:        return "ERR_INVALID_COMMAND";
        case REASON_ERR_CHECKSUM:               return "ERR_CHECKSUM";
        case REASON_ERR_BUSY:                   return "ERR_BUSY";
        default:                                return "ERR_GENERAL";
    }
}

bool Safety_IsManualOverrideActive(void) {
    return manual_override_flag;
}

void Safety_SetManualOverride(bool active) {
    manual_override_flag = active;
    if (active) {
        current_state = STATE_MANUAL_OVERRIDE;
    } else {
        current_state = STATE_SAFE_HOME;
    }
}
