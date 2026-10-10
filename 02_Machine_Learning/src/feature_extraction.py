"""
Feature Extraction Module for LumiTrack (Variant 10.03: Museum Exposure Limiter)
Extracts physically meaningful scan-level features from raw angular light readings.
"""

import numpy as np

APPROVED_ANGLES_DEG = [15, 40, 65, 90, 115, 140, 165]

def extract_features_from_scan(lux_array, target_lux=500.0, ambient_lux=40.0):
    """
    Extracts 12 physical features from a 7-point angular scan.
    
    Args:
        lux_array (list or np.ndarray): 7 calibrated lux values at approved angles.
        target_lux (float): Operator target exposure limit from potentiometer.
        ambient_lux (float): Dark/ambient reference level.
        
    Returns:
        dict: Extracted feature dictionary.
    """
    lux = np.array(lux_array, dtype=float)
    angles = np.array(APPROVED_ANGLES_DEG, dtype=float)
    
    peak_idx = int(np.argmax(lux))
    peak_intensity = float(lux[peak_idx])
    peak_angle_deg = float(angles[peak_idx])
    
    min_intensity = float(np.min(lux))
    mean_intensity = float(np.mean(lux))
    std_intensity = float(np.std(lux))
    
    # Asymmetry around safe neutral (90 deg, index 3)
    left_wing = np.sum(lux[:3])
    right_wing = np.sum(lux[4:])
    total_wing = left_wing + right_wing + 1e-5
    curve_asymmetry = float((right_wing - left_wing) / total_wing)
    
    # Spatial gradients between adjacent angle steps
    diffs = np.abs(np.diff(lux))
    angle_steps = np.diff(angles)
    gradients = diffs / angle_steps
    gradient_mean = float(np.mean(gradients))
    gradient_max = float(np.max(gradients))
    
    # Contrast ratio (glint/spotlight vs background)
    contrast_ratio = float((peak_intensity - min_intensity) / (peak_intensity + min_intensity + 1e-5))
    
    # Angular exposure integral (trapezoidal rule)
    if hasattr(np, 'trapezoid'):
        trapz_sum = np.trapezoid(lux, angles)
    else:
        trapz_sum = np.trapz(lux, angles)
    exposure_integral = float(trapz_sum)
    
    # Context ratios
    ratio_to_target = float(peak_intensity / (target_lux + 1e-5))
    ratio_to_ambient = float(peak_intensity / (ambient_lux + 1e-5))
    
    # Early slope across first 4 checkpoints (POS_0 to POS_3)
    partial_slope = float((lux[3] - lux[0]) / (angles[3] - angles[0]))
    
    return {
        "peak_intensity": peak_intensity,
        "peak_angle_deg": peak_angle_deg,
        "min_intensity": min_intensity,
        "mean_intensity": mean_intensity,
        "std_intensity": std_intensity,
        "curve_asymmetry": curve_asymmetry,
        "gradient_mean": gradient_mean,
        "gradient_max": gradient_max,
        "contrast_ratio": contrast_ratio,
        "exposure_integral": exposure_integral,
        "ratio_to_target": ratio_to_target,
        "ratio_to_ambient": ratio_to_ambient,
        "partial_slope": partial_slope
    }

def extract_partial_features(partial_lux_array, target_lux=500.0, ambient_lux=40.0):
    """
    Extracts features from an early partial scan (first 4 checkpoints: 15°, 40°, 65°, 90°)
    for confidence-aware early stopping.
    """
    lux = np.array(partial_lux_array[:4], dtype=float)
    angles = np.array(APPROVED_ANGLES_DEG[:4], dtype=float)
    
    peak_intensity = float(np.max(lux))
    mean_intensity = float(np.mean(lux))
    diffs = np.abs(np.diff(lux))
    gradient_max = float(np.max(diffs / np.diff(angles)))
    ratio_to_target = float(peak_intensity / (target_lux + 1e-5))
    partial_slope = float((lux[-1] - lux[0]) / (angles[-1] - angles[0]))
    
    return {
        "partial_peak": peak_intensity,
        "partial_mean": mean_intensity,
        "partial_gradient_max": gradient_max,
        "partial_ratio_to_target": ratio_to_target,
        "partial_slope": partial_slope
    }
