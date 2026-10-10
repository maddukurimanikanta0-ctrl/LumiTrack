"""
Unit Tests for Packet Codec and Protocol Contract
LumiTrack (Variant 10.03: Museum Exposure Limiter)
"""

import unittest
import sys
import os
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "03_IoT_Host_Application")))
from packet_codec import encode_host_command, decode_mcu_message

class TestProtocol(unittest.TestCase):
    def test_canonical_command_encoding(self):
        # Valid ANGLE_ID
        pkt = encode_host_command("ANGLE_ID", angle_id=4, seq_num=101)
        self.assertIn('"cmd": "ANGLE_ID"', pkt)
        self.assertIn('"id": 4', pkt)
        self.assertIn('"seq": 101', pkt)
        
        # Valid SCAN_PROFILE
        pkt = encode_host_command("SCAN_PROFILE", profile="FULL", seq_num=102)
        self.assertIn('"cmd": "SCAN_PROFILE"', pkt)
        self.assertIn('"profile": "FULL"', pkt)
        
        # Valid HOLD
        pkt = encode_host_command("HOLD", seq_num=103)
        self.assertIn('"cmd": "HOLD"', pkt)
        
        # Valid RETURN_HOME
        pkt = encode_host_command("RETURN_HOME", seq_num=104)
        self.assertIn('"cmd": "RETURN_HOME"', pkt)
        
    def test_invalid_command_rejection(self):
        # Disallow non-canonical synonyms (e.g., MOVE, GOTO, PARK, HOME)
        with self.assertRaises(ValueError):
            encode_host_command("GOTO", angle_id=3)
        with self.assertRaises(ValueError):
            encode_host_command("PARK")
        with self.assertRaises(ValueError):
            encode_host_command("SCAN")
            
    def test_mcu_telemetry_decoding(self):
        sample_telem = '{"type":"TELEM","variant":"10.03","run":1,"seq":15,"angle_id":3,"angle_deg":90,"ldr_raw":450,"ldr_v":0.362,"pot_raw":2048,"pot_v":1.650,"lux":85.0,"target_lux":500.0,"cum_lux":1250.0,"state":"IDLE","safe":1,"ovr":0}\r\n'
        decoded = decode_mcu_message(sample_telem)
        self.assertIsNotNone(decoded)
        self.assertEqual(decoded["type"], "TELEM")
        self.assertEqual(decoded["variant"], "10.03")
        self.assertEqual(decoded["angle_id"], 3)
        self.assertEqual(decoded["lux"], 85.0)
        self.assertEqual(decoded["state"], "IDLE")
        
    def test_mcu_ack_nack_decoding(self):
        ack_str = '{"type":"ACK","cmd":"ANGLE_ID","seq":101,"status":"ACCEPTED","reason":"OK","angle_id":4,"angle_deg":115}\r\n'
        ack = decode_mcu_message(ack_str)
        self.assertEqual(ack["type"], "ACK")
        self.assertEqual(ack["status"], "ACCEPTED")
        self.assertEqual(ack["reason"], "OK")
        
        nack_str = '{"type":"NACK","cmd":"ANGLE_ID","seq":102,"status":"REJECTED","reason":"ERR_OUT_OF_BOUNDS","curr_angle_id":3}\r\n'
        nack = decode_mcu_message(nack_str)
        self.assertEqual(nack["type"], "NACK")
        self.assertEqual(nack["status"], "REJECTED")
        self.assertEqual(nack["reason"], "ERR_OUT_OF_BOUNDS")

    def test_invalid_command_nack_decoding(self):
        # Verify decoding of ERR_INVALID_COMMAND NACK for malformed or unknown commands
        invalid_nack = '{"type":"NACK","cmd":"UNKNOWN","seq":0,"status":"REJECTED","reason":"ERR_INVALID_COMMAND","curr_angle_id":3}\r\n'
        decoded = decode_mcu_message(invalid_nack)
        self.assertEqual(decoded["type"], "NACK")
        self.assertEqual(decoded["cmd"], "UNKNOWN")
        self.assertEqual(decoded["status"], "REJECTED")
        self.assertEqual(decoded["reason"], "ERR_INVALID_COMMAND")

if __name__ == "__main__":
    unittest.main()
