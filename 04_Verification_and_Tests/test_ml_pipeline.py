"""
Unit Tests for Machine Learning Feature Extraction & Inference Pipeline
LumiTrack (Variant 10.03: Museum Exposure Limiter)
"""

import unittest
import sys
import os
import numpy as np

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "02_Machine_Learning", "src")))
from feature_extraction import extract_features_from_scan, extract_partial_features, APPROVED_ANGLES_DEG
from inference import LumiTrackPredictor

class TestMLPipeline(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        models_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "02_Machine_Learning", "models"))
        cls.predictor = LumiTrackPredictor(models_dir=models_dir)
        
    def test_feature_extraction_structure(self):
        sample_lux = [80.0, 110.0, 150.0, 175.0, 140.0, 105.0, 75.0]
        feats = extract_features_from_scan(sample_lux, target_lux=400.0, ambient_lux=35.0)
        
        expected_keys = [
            "peak_intensity", "peak_angle_deg", "min_intensity", "mean_intensity",
            "std_intensity", "curve_asymmetry", "gradient_mean", "gradient_max",
            "contrast_ratio", "exposure_integral", "ratio_to_target", "ratio_to_ambient",
            "partial_slope"
        ]
        for k in expected_keys:
            self.assertIn(k, feats)
            self.assertFalse(np.isnan(feats[k]))
            
        self.assertEqual(feats["peak_intensity"], 175.0)
        self.assertEqual(feats["peak_angle_deg"], 90.0)
        
    def test_normal_diffuse_classification(self):
        # Smooth ambient light well within target
        normal_scan = [65.0, 95.0, 140.0, 160.0, 135.0, 90.0, 60.0]
        res = self.predictor.evaluate_full_scan(normal_scan, target_lux=400.0)
        
        self.assertEqual(res["regime_name"], "NORMAL_DIFFUSE")
        self.assertEqual(res["recommended_angle_id"], 3) # Safe neutral 90 deg
        
    def test_overexposure_risk_classification(self):
        # Dangerous spotlight concentrated at POS_0 (15 deg)
        overexp_scan = [1350.0, 720.0, 210.0, 90.0, 50.0, 45.0, 40.0]
        res = self.predictor.evaluate_full_scan(overexp_scan, target_lux=400.0)
        
        self.assertEqual(res["regime_name"], "OVEREXPOSURE_RISK")
        self.assertGreaterEqual(res["confidence"], 0.85)
        self.assertEqual(res["command"], "ANGLE_ID")
        self.assertEqual(res["recommended_angle_id"], 6) # Deflected away to 165 deg!
        
    def test_specular_confuser_classification(self):
        # Very narrow glint spike at POS_2 (65 deg)
        confuser_scan = [70.0, 85.0, 550.0, 95.0, 80.0, 75.0, 70.0]
        res = self.predictor.evaluate_full_scan(confuser_scan, target_lux=400.0)
        
        self.assertEqual(res["regime_name"], "CONFUSER_SPECULAR")
        self.assertEqual(res["command"], "HOLD")
        
    def test_partial_scan_early_stopping(self):
        # High direct beam visible in early checkpoints
        overexp_early = [1400.0, 800.0, 250.0, 100.0]
        part_res = self.predictor.evaluate_partial_scan(overexp_early, target_lux=400.0)
        
        self.assertIsNotNone(part_res)
        self.assertEqual(part_res["regime_name"], "OVEREXPOSURE_RISK")
        self.assertTrue(part_res["early_stop"])
        self.assertGreaterEqual(part_res["confidence"], 0.85)

if __name__ == "__main__":
    unittest.main()
