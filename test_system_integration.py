"""
End-to-End System Integration Test
LumiTrack (Variant 10.03: Museum Exposure Limiter)
Verifies:
1. Bidirectional telemetry and state machine transitions
2. Closed-loop ML inference and canonical command contract
3. Host loss watchdog timeout (failsafe neutral homing)
4. Manual override priority and host command rejection
"""

import unittest
import sys
import os
import time

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "03_IoT_Host_Application")))
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "02_Machine_Learning", "src")))

from host_controller import LumiTrackHostController

class TestSystemIntegration(unittest.TestCase):
    def setUp(self):
        self.controller = LumiTrackHostController(mode="sim", scenario="OVEREXPOSURE_RISK")
        self.controller.connect()
        time.sleep(0.5)
        
    def tearDown(self):
        self.controller.disconnect()
        time.sleep(0.2)
        
    def test_initial_telemetry_and_state(self):
        time.sleep(0.5)
        telem = self.controller.latest_telemetry
        self.assertIsNotNone(telem)
        self.assertEqual(telem.get("type"), "TELEM")
        self.assertEqual(telem.get("variant"), "10.03")
        self.assertEqual(telem.get("state"), "SAFE_HOME")
        self.assertEqual(telem.get("angle_id"), 3)
        self.assertEqual(telem.get("angle_deg"), 90)
        
    def test_closed_loop_scan_and_classification(self):
        # Trigger scan
        self.controller.trigger_scan(profile="FULL")
        time.sleep(2.5)
        
        # Verify scan completed and ML responded
        scan_res = self.controller.latest_scan_result
        self.assertIsNotNone(scan_res)
        self.assertEqual(scan_res.get("type"), "SCAN_COMPLETE")
        self.assertEqual(scan_res.get("points"), 7)
        
        # Verify host received ACK for safe deflected angle
        time.sleep(0.5)
        last_ack = self.controller.last_ack_response
        self.assertIsNotNone(last_ack)
        self.assertEqual(last_ack.get("status"), "ACCEPTED")
        self.assertEqual(last_ack.get("reason"), "OK")
        
    def test_watchdog_timeout_on_host_loss(self):
        # Move to a non-home position first
        self.controller.set_angle(5)
        time.sleep(0.5)
        self.assertEqual(self.controller.virtual_mcu.target_angle_id, 5)
        
        # Cease all host commands and simulate host disappearance (>3.0 seconds)
        print("\n[Test] Simulating host link loss for 3.5 seconds...")
        time.sleep(3.5)
        
        # Verify virtual MCU watchdog triggered and moved back to safe neutral
        mcu_state = self.controller.virtual_mcu.state
        mcu_angle = self.controller.virtual_mcu.target_angle_id
        
        self.assertEqual(mcu_state, "FAULT_HOST_TIMEOUT")
        self.assertEqual(mcu_angle, 3) # Safe Home POS_3 (90°)
        print("[Test] Verified: MCU transitioned to FAULT_HOST_TIMEOUT and homed safely to POS_3 (90°).")
        
    def test_manual_override_priority(self):
        # Engage manual override on MCU
        self.controller.virtual_mcu.manual_override = True
        time.sleep(0.2)
        
        # Try to send host ANGLE_ID command
        self.controller.set_angle(2)
        time.sleep(0.5)
        
        # Verify MCU NACKed the command
        last_ack = self.controller.last_ack_response
        self.assertIsNotNone(last_ack)
        self.assertEqual(last_ack.get("status"), "REJECTED")
        self.assertEqual(last_ack.get("reason"), "ERR_MANUAL_OVERRIDE_ACTIVE")
        print("[Test] Verified: MCU rejected host command during manual override.")

if __name__ == "__main__":
    reports_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "test_reports")
    log_file = os.path.join(reports_dir, "test_results.log")
    
    with open(log_file, "w") as f:
        runner = unittest.TextTestRunner(stream=f, verbosity=2)
        suite = unittest.TestLoader().loadTestsFromTestCase(TestSystemIntegration)
        result = runner.run(suite)
        
    print(f"\nIntegration test results written to {log_file}")
    if result.wasSuccessful():
        print("ALL INTEGRATION TESTS PASSED (4/4)!")
    else:
        print(f"FAILED: {len(result.failures)} failures, {len(result.errors)} errors.")
