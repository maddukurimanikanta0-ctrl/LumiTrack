/**
  ******************************************************************************
  * @file    protocol.c
  * @brief   Canonical command protocol parser, telemetry encoder, and ACK/NACK
  ******************************************************************************
  */

#include "protocol.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define RX_BUFFER_SIZE  256
#define TX_BUFFER_SIZE  512

static UART_HandleTypeDef *s_huart = NULL;

static char s_rx_buffer[RX_BUFFER_SIZE];
static uint16_t s_rx_index = 0;
static bool s_cmd_ready = false;
static HostCommand_t s_pending_cmd;

static void SendString(const char *str) {
    if (s_huart != NULL && str != NULL) {
        HAL_UART_Transmit(s_huart, (uint8_t*)str, strlen(str), 100);
    }
}

void Protocol_Init(UART_HandleTypeDef *huart) {
    s_huart = huart;
    s_rx_index = 0;
    s_cmd_ready = false;
    memset(&s_pending_cmd, 0, sizeof(HostCommand_t));
}

void Protocol_ReceiveByte(uint8_t rx_char) {
    if (s_cmd_ready) {
        /* Buffer busy with unhandled command, drop or overwrite */
        return;
    }

    if (rx_char == '\r' || rx_char == '\n') {
        if (s_rx_index > 0) {
            s_rx_buffer[s_rx_index] = '\0';
            
            /* Parse the received line */
            char *line = s_rx_buffer;
            
            /* Reset defaults */
            memset(&s_pending_cmd, 0, sizeof(HostCommand_t));
            s_pending_cmd.type = CMD_TYPE_NONE;
            s_pending_cmd.confidence = 1.0f;
            s_pending_cmd.received_tick = HAL_GetTick();
            strncpy(s_pending_cmd.model_version, "v1.0", sizeof(s_pending_cmd.model_version) - 1);

            /* Check for ANGLE_ID */
            if (strstr(line, "ANGLE_ID") != NULL) {
                s_pending_cmd.type = CMD_TYPE_ANGLE_ID;
                char *p = strstr(line, "ANGLE_ID");
                /* Try reading id from 'ANGLE_ID <val>' or '"id": <val>' or 'id=<val>' */
                int id = -1;
                if (strstr(line, "\"id\":") != NULL) {
                    sscanf(strstr(line, "\"id\":"), "\"id\":%d", &id);
                } else if (strstr(line, "id=") != NULL) {
                    sscanf(strstr(line, "id="), "id=%d", &id);
                } else {
                    sscanf(p, "ANGLE_ID %d", &id);
                }
                s_pending_cmd.angle_id = (id >= 0) ? (uint8_t)id : 255;
            }
            /* Check for SCAN_PROFILE */
            else if (strstr(line, "SCAN_PROFILE") != NULL) {
                s_pending_cmd.type = CMD_TYPE_SCAN_PROFILE;
                if (strstr(line, "PARTIAL") != NULL) {
                    s_pending_cmd.profile = SCAN_PROFILE_PARTIAL;
                } else if (strstr(line, "FAST") != NULL) {
                    s_pending_cmd.profile = SCAN_PROFILE_FAST;
                } else {
                    s_pending_cmd.profile = SCAN_PROFILE_FULL;
                }
            }
            /* Check for HOLD */
            else if (strstr(line, "HOLD") != NULL) {
                s_pending_cmd.type = CMD_TYPE_HOLD;
            }
            /* Check for RETURN_HOME */
            else if (strstr(line, "RETURN_HOME") != NULL) {
                s_pending_cmd.type = CMD_TYPE_RETURN_HOME;
            }

            /* Parse optional sequence number */
            if (strstr(line, "seq=") != NULL) {
                sscanf(strstr(line, "seq="), "seq=%lu", &s_pending_cmd.seq_num);
            } else if (strstr(line, "\"seq\":") != NULL) {
                sscanf(strstr(line, "\"seq\":"), "\"seq\":%lu", &s_pending_cmd.seq_num);
            }

            /* Parse optional expiry */
            if (strstr(line, "exp=") != NULL) {
                sscanf(strstr(line, "exp="), "exp=%lu", &s_pending_cmd.expiry_ms);
            } else if (strstr(line, "\"exp\":") != NULL) {
                sscanf(strstr(line, "\"exp\":"), "\"exp\":%lu", &s_pending_cmd.expiry_ms);
            }

            /* Parse optional confidence */
            if (strstr(line, "conf=") != NULL) {
                sscanf(strstr(line, "conf="), "conf=%f", &s_pending_cmd.confidence);
            } else if (strstr(line, "\"conf\":") != NULL) {
                sscanf(strstr(line, "\"conf\":"), "\"conf\":%f", &s_pending_cmd.confidence);
            }

            /* Mark command ready (even if CMD_TYPE_NONE, so main loop can send NACK ERR_INVALID_COMMAND) */
            s_cmd_ready = true;
            s_rx_index = 0;
        }
    } else {
        if (s_rx_index < (RX_BUFFER_SIZE - 1)) {
            s_rx_buffer[s_rx_index++] = (char)rx_char;
        }
    }
}

bool Protocol_HasPendingCommand(void) {
    return s_cmd_ready;
}

bool Protocol_GetCommand(HostCommand_t *cmd) {
    if (!s_cmd_ready) {
        return false;
    }
    memcpy(cmd, &s_pending_cmd, sizeof(HostCommand_t));
    s_cmd_ready = false;
    return true;
}

static const char* GetCmdTypeName(CommandType_t type) {
    switch (type) {
        case CMD_TYPE_ANGLE_ID:     return "ANGLE_ID";
        case CMD_TYPE_SCAN_PROFILE: return "SCAN_PROFILE";
        case CMD_TYPE_HOLD:         return "HOLD";
        case CMD_TYPE_RETURN_HOME:  return "RETURN_HOME";
        default:                    return "UNKNOWN";
    }
}

void Protocol_SendAck(const HostCommand_t *cmd, AckReason_t reason, uint8_t current_angle_id, uint16_t current_angle_deg) {
    char tx_buf[TX_BUFFER_SIZE];
    snprintf(tx_buf, sizeof(tx_buf),
             "{\"type\":\"ACK\",\"cmd\":\"%s\",\"seq\":%lu,\"status\":\"ACCEPTED\",\"reason\":\"%s\",\"angle_id\":%u,\"angle_deg\":%u}\r\n",
             GetCmdTypeName(cmd->type),
             cmd->seq_num,
             Safety_GetReasonString(reason),
             current_angle_id,
             current_angle_deg);
    SendString(tx_buf);
}

void Protocol_SendNack(const HostCommand_t *cmd, AckReason_t reason, uint8_t current_angle_id) {
    char tx_buf[TX_BUFFER_SIZE];
    snprintf(tx_buf, sizeof(tx_buf),
             "{\"type\":\"NACK\",\"cmd\":\"%s\",\"seq\":%lu,\"status\":\"REJECTED\",\"reason\":\"%s\",\"curr_angle_id\":%u}\r\n",
             GetCmdTypeName(cmd->type),
             cmd->seq_num,
             Safety_GetReasonString(reason),
             current_angle_id);
    SendString(tx_buf);
}

void Protocol_SendTelemetry(uint32_t run_id, uint32_t seq, uint8_t angle_id, uint16_t angle_deg, const SensorData_t *sensors, SystemState_t state) {
    char tx_buf[TX_BUFFER_SIZE];
    snprintf(tx_buf, sizeof(tx_buf),
             "{\"type\":\"TELEM\",\"variant\":\"%s\",\"run\":%lu,\"seq\":%lu,\"angle_id\":%u,\"angle_deg\":%u,"
             "\"ldr_raw\":%u,\"ldr_v\":%.3f,\"pot_raw\":%u,\"pot_v\":%.3f,\"lux\":%.1f,\"target_lux\":%.1f,"
             "\"cum_lux\":%.1f,\"state\":\"%s\",\"safe\":%d,\"ovr\":%d}\r\n",
             VARIANT_ID,
             run_id,
             seq,
             angle_id,
             angle_deg,
             sensors->ldr_raw,
             sensors->ldr_voltage,
             sensors->pot_raw,
             sensors->pot_voltage,
             sensors->lux_estimate,
             sensors->target_threshold,
             sensors->cumulative_exposure,
             Safety_GetStateString(state),
             (state == STATE_SAFE_HOME || state == STATE_POSITIONED || state == STATE_IDLE) ? 1 : 0,
             Safety_IsManualOverrideActive() ? 1 : 0);
    SendString(tx_buf);
}

void Protocol_SendScanComplete(ScanProfile_t profile, uint8_t num_points, const uint16_t *angles_deg, const float *lux_values) {
    char tx_buf[TX_BUFFER_SIZE];
    const char *prof_str = (profile == SCAN_PROFILE_PARTIAL) ? "PARTIAL" : (profile == SCAN_PROFILE_FAST) ? "FAST" : "FULL";
    
    int len = snprintf(tx_buf, sizeof(tx_buf), "{\"type\":\"SCAN_COMPLETE\",\"profile\":\"%s\",\"points\":%u,\"data\":[", prof_str, num_points);
    
    for (uint8_t i = 0; i < num_points; i++) {
        char point_buf[32];
        snprintf(point_buf, sizeof(point_buf), "{\"deg\":%u,\"lux\":%.1f}%s",
                 angles_deg[i], lux_values[i], (i == num_points - 1) ? "" : ",");
        if ((len + strlen(point_buf) + 5) < sizeof(tx_buf)) {
            strcat(tx_buf, point_buf);
            len += strlen(point_buf);
        }
    }
    strcat(tx_buf, "]}\r\n");
    SendString(tx_buf);
}
