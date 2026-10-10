"""
Packet Codec and Protocol Formatter
LumiTrack (Variant 10.03: Museum Exposure Limiter)
Strictly adheres to Parent Problem 10 canonical command contract:
ANGLE_ID | SCAN_PROFILE | HOLD | RETURN_HOME
"""

import json
import time

CANONICAL_COMMANDS = ["ANGLE_ID", "SCAN_PROFILE", "HOLD", "RETURN_HOME"]

def encode_host_command(cmd_type, angle_id=None, profile=None, seq_num=1, model_ver="1.0", confidence=1.0, expiry_ms=5000):
    """
    Encodes a canonical host command packet into JSON string for UART transmission.
    """
    if cmd_type not in CANONICAL_COMMANDS:
        raise ValueError(f"Invalid canonical command '{cmd_type}'. Must be one of {CANONICAL_COMMANDS}")
        
    payload = {
        "cmd": cmd_type,
        "seq": int(seq_num),
        "model_ver": str(model_ver),
        "conf": round(float(confidence), 3),
        "exp": int(expiry_ms)
    }
    
    if cmd_type == "ANGLE_ID":
        if angle_id is None:
            raise ValueError("ANGLE_ID command requires 'angle_id' (0 to 6)")
        payload["id"] = int(angle_id)
    elif cmd_type == "SCAN_PROFILE":
        payload["profile"] = str(profile or "FULL")
        
    packet_str = json.dumps(payload) + "\r\n"
    return packet_str

def decode_mcu_message(line):
    """
    Decodes an incoming line from the STM32 MCU.
    Supports JSON packets for TELEM, ACK, NACK, and SCAN_COMPLETE.
    """
    if not line:
        return None
        
    line_clean = line.strip()
    if not line_clean:
        return None
        
    try:
        data = json.loads(line_clean)
        return data
    except json.JSONDecodeError:
        # Non-JSON debug output
        return {"type": "RAW_DEBUG", "message": line_clean}
