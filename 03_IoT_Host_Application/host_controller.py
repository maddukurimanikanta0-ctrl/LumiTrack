"""
Host Controller and IoT Communication Engine
LumiTrack (Variant 10.03: Museum Exposure Limiter)
Connects to STM32 NUCLEO-F446RE over Serial UART (or Virtual STM32 Emulator),
manages bidirectional telemetry, keep-alive heartbeats, and closed-loop ML control.
"""

import sys
import os
import time
import threading
import queue

# Add paths
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "02_Machine_Learning", "src"))

from packet_codec import encode_host_command, decode_mcu_message
from virtual_stm32 import VirtualSTM32
from inference import LumiTrackPredictor

class LumiTrackHostController:
    def __init__(self, mode="sim", port=None, baudrate=115200, scenario="OVEREXPOSURE_RISK"):
        self.mode = mode
        self.port = port
        self.baudrate = baudrate
        self.scenario = scenario
        
        self.serial_conn = None
        self.virtual_mcu = None
        
        self.seq_counter = 100
        self.latest_telemetry = {}
        self.latest_scan_result = None
        self.last_ack_response = None
        
        self.running = False
        self._rx_thread = None
        self._lock = threading.Lock()
        
        self.predictor = None
        try:
            self.predictor = LumiTrackPredictor()
        except Exception as e:
            print(f"[Warning] Could not initialize ML predictor: {e}")
            
    def connect(self):
        self.running = True
        if self.mode == "live":
            import serial
            print(f"[Host] Opening Serial Port {self.port} at {self.baudrate} baud...")
            self.serial_conn = serial.Serial(self.port, self.baudrate, timeout=0.1)
            time.sleep(1.0) # Settle ST-LINK
        else:
            print(f"[Host] Launching Virtual STM32 NUCLEO-F446RE Emulator (Scenario: {self.scenario})...")
            self.virtual_mcu = VirtualSTM32(scenario=self.scenario)
            self.virtual_mcu.start()
            
        self._rx_thread = threading.Thread(target=self._rx_worker, daemon=True)
        self._rx_thread.start()
        print("[Host] Communication bridge established.")
        
    def disconnect(self):
        self.running = False
        if self.mode == "live" and self.serial_conn:
            self.serial_conn.close()
        elif self.virtual_mcu:
            self.virtual_mcu.stop()
        print("[Host] Disconnected.")
        
    def _send_raw_line(self, line):
        if self.mode == "live" and self.serial_conn:
            self.serial_conn.write(line.encode("utf-8"))
        elif self.virtual_mcu:
            self.virtual_mcu.write_from_host(line)
            
    def send_command(self, cmd_type, angle_id=None, profile=None, confidence=1.0, expiry_ms=5000):
        with self._lock:
            self.seq_counter += 1
            seq = self.seq_counter
            
        pkt = encode_host_command(
            cmd_type=cmd_type,
            angle_id=angle_id,
            profile=profile,
            seq_num=seq,
            model_ver="1.0",
            confidence=confidence,
            expiry_ms=expiry_ms
        )
        self._send_raw_line(pkt)
        return seq
        
    def trigger_scan(self, profile="FULL"):
        print(f"[Host] Requesting SCAN_PROFILE: {profile}")
        return self.send_command("SCAN_PROFILE", profile=profile)
        
    def set_angle(self, angle_id, confidence=1.0):
        print(f"[Host] Commanding ANGLE_ID: {angle_id}")
        return self.send_command("ANGLE_ID", angle_id=angle_id, confidence=confidence)
        
    def hold(self):
        return self.send_command("HOLD")
        
    def return_home(self):
        print("[Host] Commanding RETURN_HOME (Safe Neutral)")
        return self.send_command("RETURN_HOME")
        
    def _rx_worker(self):
        line_buffer = ""
        while self.running:
            raw_lines = []
            if self.mode == "live" and self.serial_conn:
                try:
                    if self.serial_conn.in_waiting > 0:
                        chunk = self.serial_conn.read(self.serial_conn.in_waiting).decode("utf-8", errors="replace")
                        line_buffer += chunk
                        while "\n" in line_buffer:
                            line, line_buffer = line_buffer.split("\n", 1)
                            raw_lines.append(line.strip())
                except Exception:
                    pass
            elif self.virtual_mcu:
                line = self.virtual_mcu.read_to_host(block=True, timeout=0.05)
                if line:
                    raw_lines.append(line.strip())
                    
            if raw_lines:
                for line in raw_lines:
                    if not line:
                        continue
                    msg = decode_mcu_message(line)
                    if msg:
                        msg_type = msg.get("type")
                        if msg_type == "TELEM":
                            with self._lock:
                                self.latest_telemetry = msg
                        elif msg_type == "SCAN_COMPLETE":
                            with self._lock:
                                self.latest_scan_result = msg
                            self._on_scan_complete(msg)
                        elif msg_type in ["ACK", "NACK"]:
                            with self._lock:
                                self.last_ack_response = msg
                            status = msg.get("status")
                            reason = msg.get("reason")
                            print(f"[MCU Response] {msg_type}: {status} ({reason})")
            else:
                time.sleep(0.01)
                
    def _on_scan_complete(self, scan_msg):
        """Called whenever the STM32 finishes an angular scan"""
        data_points = scan_msg.get("data", [])
        if not data_points or self.predictor is None:
            return
            
        lux_values = [p["lux"] for p in data_points]
        target_lux = self.latest_telemetry.get("target_lux", 500.0)
        ambient_lux = 40.0
        
        print(f"\n[Host ML] Processing completed scan ({len(lux_values)} points)...")
        res = self.predictor.evaluate_full_scan(lux_values, target_lux, ambient_lux)
        
        regime = res["regime_name"]
        conf = res["confidence"]
        cmd = res["command"]
        rec_angle = res["recommended_angle_id"]
        rec_deg = res["recommended_angle_deg"]
        
        print(f"[Host ML Result] Classified Regime: {regime} (Confidence: {conf*100:.1f}%)")
        print(f"[Host Action] Recommendation: {cmd} -> Angle ID {rec_angle} ({rec_deg}°)")
        
        # Dispatch canonical command
        if cmd == "ANGLE_ID":
            self.set_angle(rec_angle, confidence=conf)
        elif cmd == "HOLD":
            self.hold()
        else:
            self.return_home()
            
    def get_status_summary(self):
        with self._lock:
            telem = dict(self.latest_telemetry)
            ack = dict(self.last_ack_response) if self.last_ack_response else {}
        return {
            "mode": self.mode,
            "connected": self.running,
            "telemetry": telem,
            "last_ack": ack
        }

if __name__ == "__main__":
    controller = LumiTrackHostController(mode="sim", scenario="OVEREXPOSURE_RISK")
    controller.connect()
    
    print("\n[Test] Waiting 1 second for initial telemetry...")
    time.sleep(1.0)
    print("Latest Telem:", controller.latest_telemetry)
    
    print("\n[Test] Triggering angular scan...")
    controller.trigger_scan(profile="FULL")
    time.sleep(3.0)
    
    print("\n[Test] Testing Safe Home...")
    controller.return_home()
    time.sleep(1.0)
    
    controller.disconnect()
    print("[Test] Complete.")
