"""
Real-Time Inference Engine for LumiTrack (Variant 10.03: Museum Exposure Limiter)
Loads serialized models and provides prediction and decision-making for host application.
"""

import os
import joblib
import numpy as np
import pandas as pd
from feature_extraction import extract_features_from_scan, extract_partial_features, APPROVED_ANGLES_DEG

REGIME_NAMES = {
    0: "NORMAL_DIFFUSE",
    1: "OVEREXPOSURE_RISK",
    2: "CONFUSER_SPECULAR"
}

class LumiTrackPredictor:
    def __init__(self, models_dir=None):
        if models_dir is None:
            models_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "models")
            
        self.best_model_path = os.path.join(models_dir, "best_model.joblib")
        self.partial_model_path = os.path.join(models_dir, "partial_model.joblib")
        
        if not os.path.exists(self.best_model_path):
            raise FileNotFoundError(f"Model file not found at {self.best_model_path}. Run train_models.py first.")
            
        self.full_model = joblib.load(self.best_model_path)
        self.partial_model = joblib.load(self.partial_model_path) if os.path.exists(self.partial_model_path) else None
        
    def evaluate_full_scan(self, lux_array, target_lux=500.0, ambient_lux=40.0):
        """
        Evaluates a complete 7-point scan and determines canonical action.
        """
        feats = extract_features_from_scan(lux_array, target_lux, ambient_lux)
        X = pd.DataFrame([feats])
        
        pred_class = int(self.full_model.predict(X)[0])
        probas = self.full_model.predict_proba(X)[0]
        confidence = float(probas[pred_class])
        
        # Safe Angle Decision Policy:
        # Class 0 (NORMAL): Keep artifact viewable at safe neutral (POS_3, 90 deg)
        # Class 1 (OVEREXPOSURE): Direct beam hazard! Turn sensor/louver away from peak!
        # Class 2 (CONFUSER): False alarm glint! Maintain stable HOLD at safe neutral
        if pred_class == 1: # OVEREXPOSURE_RISK
            peak_idx = int(np.argmax(lux_array))
            if peak_idx <= 2:
                rec_angle_id = 6 # Deflect to 165 deg
            elif peak_idx >= 4:
                rec_angle_id = 0 # Deflect to 15 deg
            else:
                rec_angle_id = 0 # Deflect
            cmd = "ANGLE_ID"
        elif pred_class == 2: # CONFUSER_SPECULAR
            rec_angle_id = 3 # Safe neutral
            cmd = "HOLD"
        else: # NORMAL_DIFFUSE
            rec_angle_id = 3 # Safe neutral
            cmd = "ANGLE_ID"
            
        return {
            "regime_class": pred_class,
            "regime_name": REGIME_NAMES[pred_class],
            "confidence": round(confidence, 3),
            "command": cmd,
            "recommended_angle_id": rec_angle_id,
            "recommended_angle_deg": APPROVED_ANGLES_DEG[rec_angle_id],
            "features": feats
        }
        
    def evaluate_partial_scan(self, partial_lux_array, target_lux=500.0, ambient_lux=40.0, conf_threshold=0.85):
        """
        Evaluates an early 4-point scan (POS_0 to POS_3) for early stopping.
        """
        if self.partial_model is None or len(partial_lux_array) < 4:
            return None
            
        feats = extract_partial_features(partial_lux_array, target_lux, ambient_lux)
        X = pd.DataFrame([feats])
        
        pred_class = int(self.partial_model.predict(X)[0])
        probas = self.partial_model.predict_proba(X)[0]
        confidence = float(probas[pred_class])
        
        can_stop_early = (confidence >= conf_threshold) and (pred_class == 1)
        
        return {
            "early_stop": can_stop_early,
            "regime_class": pred_class,
            "regime_name": REGIME_NAMES[pred_class],
            "confidence": round(confidence, 3),
            "recommended_angle_id": 0 if can_stop_early else 3,
            "features": feats
        }
