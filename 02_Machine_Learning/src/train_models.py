"""
Model Training and Baseline Comparison Pipeline
LumiTrack (Variant 10.03: Museum Exposure Limiter)

Compares:
1. Parent Rule-Based Heuristic Baseline (Exhaustive Scan + Brightest/Threshold)
2. Ridge Logistic Regression (Syllabus M2)
3. Random Forest Classifier (Syllabus M3)
4. Gradient Boosting Classifier (Syllabus M3)
5. Confidence-Aware Partial-Scan Early Stopping Model
"""

import os
import json
import joblib
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

from sklearn.model_selection import GroupShuffleSplit
from sklearn.preprocessing import StandardScaler
from sklearn.linear_model import LogisticRegression
from sklearn.ensemble import RandomForestClassifier, GradientBoostingClassifier
from sklearn.metrics import classification_report, confusion_matrix, f1_score, accuracy_score, recall_score
from sklearn.pipeline import Pipeline

from feature_extraction import extract_features_from_scan, extract_partial_features, APPROVED_ANGLES_DEG

def run_training_pipeline():
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    data_path = os.path.join(base_dir, "data", "seed_scan_dataset.csv")
    models_dir = os.path.join(base_dir, "models")
    reports_dir = os.path.join(os.path.dirname(base_dir), "04_Verification_and_Tests", "test_reports")
    os.makedirs(models_dir, exist_ok=True)
    os.makedirs(reports_dir, exist_ok=True)
    
    print(f"Loading seed dataset from {data_path}...")
    df = pd.read_csv(data_path)
    
    # -------------------------------------------------------------------------
    # 1. Feature Extraction
    # -------------------------------------------------------------------------
    print("Extracting physical scan features...")
    full_feature_rows = []
    partial_feature_rows = []
    
    lux_cols = [f"lux_pos{i}_{deg}deg" for i, deg in enumerate(APPROVED_ANGLES_DEG)]
    
    for _, row in df.iterrows():
        lux_vals = [row[c] for c in lux_cols]
        target_lux = row["operator_target_lux"]
        amb_lux = row["ambient_ref_lux"]
        
        full_feats = extract_features_from_scan(lux_vals, target_lux, amb_lux)
        full_feature_rows.append(full_feats)
        
        part_feats = extract_partial_features(lux_vals, target_lux, amb_lux)
        partial_feature_rows.append(part_feats)
        
    X_full = pd.DataFrame(full_feature_rows)
    X_part = pd.DataFrame(partial_feature_rows)
    y = df["regime_class"].values
    groups = df["session_id"].values
    
    # -------------------------------------------------------------------------
    # 2. Group Split by Session (Preventing Data Leakage Across Adjacent Rows)
    # -------------------------------------------------------------------------
    gss = GroupShuffleSplit(n_splits=1, test_size=0.25, random_state=42)
    train_idx, test_idx = next(gss.split(X_full, y, groups=groups))
    
    X_train, X_test = X_full.iloc[train_idx], X_full.iloc[test_idx]
    y_train, y_test = y[train_idx], y[test_idx]
    
    X_part_train, X_part_test = X_part.iloc[train_idx], X_part.iloc[test_idx]
    
    print(f"Dataset split: {len(train_idx)} train samples, {len(test_idx)} test samples.")
    print(f"Held-out test sessions: {np.unique(groups[test_idx])}")
    
    # -------------------------------------------------------------------------
    # 3. Baseline Model (Parent Rule-Based Threshold Heuristic)
    # -------------------------------------------------------------------------
    # Rule: If peak_intensity > target_lux -> class 1 (OVEREXPOSURE), else class 0 (NORMAL)
    # Inability: Baseline completely fails to recognize class 2 (CONFUSER_SPECULAR)!
    y_pred_baseline = []
    for _, row in X_test.iterrows():
        if row["peak_intensity"] > 400.0:
            y_pred_baseline.append(1) # OVEREXPOSURE
        else:
            y_pred_baseline.append(0) # NORMAL
    y_pred_baseline = np.array(y_pred_baseline)
    
    # -------------------------------------------------------------------------
    # 4. Model A: Ridge Logistic Regression (L2 Regularization)
    # -------------------------------------------------------------------------
    pipe_ridge = Pipeline([
        ("scaler", StandardScaler()),
        ("clf", LogisticRegression(C=1.0, max_iter=1000, random_state=42))
    ])
    pipe_ridge.fit(X_train, y_train)
    y_pred_ridge = pipe_ridge.predict(X_test)
    
    # -------------------------------------------------------------------------
    # 5. Model B: Random Forest Classifier
    # -------------------------------------------------------------------------
    rf_clf = RandomForestClassifier(n_estimators=100, max_depth=6, random_state=42)
    rf_clf.fit(X_train, y_train)
    y_pred_rf = rf_clf.predict(X_test)
    
    # -------------------------------------------------------------------------
    # 6. Model C: Gradient Boosting Classifier
    # -------------------------------------------------------------------------
    gb_clf = GradientBoostingClassifier(n_estimators=100, learning_rate=0.1, max_depth=4, random_state=42)
    gb_clf.fit(X_train, y_train)
    y_pred_gb = gb_clf.predict(X_test)
    
    # -------------------------------------------------------------------------
    # 7. Model D: Partial-Scan Early Stopping Model (POS_0 to POS_3)
    # -------------------------------------------------------------------------
    pipe_part = Pipeline([
        ("scaler", StandardScaler()),
        ("clf", RandomForestClassifier(n_estimators=100, max_depth=5, random_state=42))
    ])
    pipe_part.fit(X_part_train, y_train)
    y_pred_part = pipe_part.predict(X_part_test)
    y_prob_part = pipe_part.predict_proba(X_part_test)
    
    # Calculate early stopping metrics:
    # How many times was confidence >= 0.85 after 4 points, saving 3 points (42.8% time saved)?
    confident_mask = np.max(y_prob_part, axis=1) >= 0.85
    early_stop_rate = float(np.mean(confident_mask))
    time_saved_pct = float(early_stop_rate * (3.0 / 7.0) * 100.0)
    
    # -------------------------------------------------------------------------
    # 8. Metrics Computation & Comparison
    # -------------------------------------------------------------------------
    def calc_metrics(y_true, y_pred, name):
        acc = accuracy_score(y_true, y_pred)
        f1_macro = f1_score(y_true, y_pred, average="macro")
        
        # Operational Risk (Class 1: OVEREXPOSURE_RISK) Recall & False Alarm
        cm = confusion_matrix(y_true, y_pred, labels=[0, 1, 2])
        # Recall for Class 1: TP_1 / (TP_1 + FN_1)
        rec_class1 = float(cm[1, 1] / (np.sum(cm[1, :]) + 1e-5))
        # False alarm for Class 1: FP_1 / (TN_1 + FP_1)
        fp_class1 = float((cm[0, 1] + cm[2, 1]) / (np.sum(cm[0, :]) + np.sum(cm[2, :]) + 1e-5))
        
        return {
            "model_name": name,
            "accuracy": round(float(acc), 4),
            "macro_f1": round(float(f1_macro), 4),
            "overexposure_recall": round(rec_class1, 4),
            "overexposure_false_alarm": round(fp_class1, 4),
            "confusion_matrix": cm.tolist()
        }
        
    metrics_baseline = calc_metrics(y_test, y_pred_baseline, "Parent Rule-Based Baseline")
    metrics_ridge = calc_metrics(y_test, y_pred_ridge, "Ridge Logistic Regression")
    metrics_rf = calc_metrics(y_test, y_pred_rf, "Random Forest Classifier")
    metrics_gb = calc_metrics(y_test, y_pred_gb, "Gradient Boosting Classifier")
    metrics_part = calc_metrics(y_test, y_pred_part, "Partial-Scan Early Stopping Model (4/7 Checkpoints)")
    
    summary = {
        "test_sample_count": len(y_test),
        "held_out_sessions": list(np.unique(groups[test_idx])),
        "comparison_table": [
            metrics_baseline,
            metrics_ridge,
            metrics_rf,
            metrics_gb,
            metrics_part
        ],
        "partial_scan_savings": {
            "confidence_threshold": 0.85,
            "early_stopping_trigger_rate": round(early_stop_rate, 4),
            "total_scan_time_reduction_pct": round(time_saved_pct, 2)
        }
    }
    
    # Save Metrics JSON
    summary_path = os.path.join(models_dir, "metrics_summary.json")
    with open(summary_path, "w") as f:
        json.dump(summary, f, indent=2)
    print(f"Metrics saved to {summary_path}")
    
    # Save Serialized Models
    best_model_path = os.path.join(models_dir, "best_model.joblib")
    joblib.dump(rf_clf, best_model_path)
    
    partial_model_path = os.path.join(models_dir, "partial_model.joblib")
    joblib.dump(pipe_part, partial_model_path)
    
    ridge_model_path = os.path.join(models_dir, "ridge_model.joblib")
    joblib.dump(pipe_ridge, ridge_model_path)
    print(f"Models serialized to {best_model_path} and {partial_model_path}")
    
    # -------------------------------------------------------------------------
    # 9. Plotting Confusion Matrix
    # -------------------------------------------------------------------------
    fig, axes = plt.subplots(1, 2, figsize=(14, 5))
    class_names = ["Normal Diffuse", "Overexposure Risk", "Specular Confuser"]
    
    # Baseline CM
    cm_base = np.array(metrics_baseline["confusion_matrix"])
    im0 = axes[0].imshow(cm_base, interpolation="nearest", cmap="Blues")
    axes[0].set_title(f"Baseline Heuristic\nMacro-F1: {metrics_baseline['macro_f1']:.3f}")
    axes[0].set_xticks(range(3))
    axes[0].set_yticks(range(3))
    axes[0].set_xticklabels(class_names, rotation=20)
    axes[0].set_yticklabels(class_names)
    axes[0].set_xlabel("Predicted")
    axes[0].set_ylabel("Actual Ground Truth")
    for i in range(3):
        for j in range(3):
            axes[0].text(j, i, str(cm_base[i, j]), ha="center", va="center", color="red" if i!=j and cm_base[i, j]>0 else "black")
            
    # Random Forest CM
    cm_rf = np.array(metrics_rf["confusion_matrix"])
    im1 = axes[1].imshow(cm_rf, interpolation="nearest", cmap="Greens")
    axes[1].set_title(f"Random Forest (Variant 10.03)\nMacro-F1: {metrics_rf['macro_f1']:.3f} | Recall: {metrics_rf['overexposure_recall']*100:.1f}%")
    axes[1].set_xticks(range(3))
    axes[1].set_yticks(range(3))
    axes[1].set_xticklabels(class_names, rotation=20)
    axes[1].set_yticklabels(class_names)
    axes[1].set_xlabel("Predicted")
    axes[1].set_ylabel("Actual Ground Truth")
    for i in range(3):
        for j in range(3):
            axes[1].text(j, i, str(cm_rf[i, j]), ha="center", va="center", color="red" if i!=j and cm_rf[i, j]>0 else "black")
            
    plt.tight_layout()
    cm_plot_path = os.path.join(reports_dir, "confusion_matrix.png")
    plt.savefig(cm_plot_path, dpi=150)
    plt.close()
    print(f"Confusion matrix plot saved to {cm_plot_path}")
    
    # -------------------------------------------------------------------------
    # 10. Plotting Synchronized Decision Timeline
    # -------------------------------------------------------------------------
    fig, (ax_top, ax_mid, ax_bot) = plt.subplots(3, 1, figsize=(12, 8), sharex=True)
    
    sample_runs = 25
    x_axis = np.arange(1, sample_runs + 1)
    
    sample_peak_lux = X_test["peak_intensity"].iloc[:sample_runs].values
    sample_target = [400.0] * sample_runs
    sample_actual = y_test[:sample_runs]
    sample_pred = y_pred_rf[:sample_runs]
    
    # Top: Light intensity vs Potentiometer Threshold
    ax_top.plot(x_axis, sample_peak_lux, 'b-o', label="Peak Measured Lux")
    ax_top.axhline(400.0, color='r', linestyle='--', label="Operator Conservation Threshold (400 lux)")
    ax_top.set_ylabel("Intensity (Lux)")
    ax_top.set_title("LumiTrack Synchronized Physical-Signal Decision Timeline")
    ax_top.grid(True, alpha=0.3)
    ax_top.legend(loc="upper right")
    
    # Mid: Predicted Regime Class
    ax_mid.step(x_axis, sample_actual, where='mid', label="Ground Truth Regime", color='black', alpha=0.6)
    ax_mid.plot(x_axis, sample_pred, 'g^', label="Model Classification", markersize=8)
    ax_mid.set_yticks([0, 1, 2])
    ax_mid.set_yticklabels(["Normal", "Overexposure", "Confuser"])
    ax_mid.set_ylabel("Class")
    ax_mid.grid(True, alpha=0.3)
    ax_mid.legend(loc="upper right")
    
    # Bottom: MCU Commanded Angle Outcome
    sample_angles = []
    for p in sample_pred:
        if p == 1:
            sample_angles.append(15) # Clamped away / deflected
        else:
            sample_angles.append(90) # Safe neutral
    ax_bot.step(x_axis, sample_angles, where='mid', color='purple', label="MCU Commanded Safe Angle (°)")
    ax_bot.set_yticks([15, 40, 65, 90, 115, 140, 165])
    ax_bot.set_ylabel("Servo Angle (°)")
    ax_bot.set_xlabel("Trial Run Number")
    ax_bot.grid(True, alpha=0.3)
    ax_bot.legend(loc="upper right")
    
    plt.tight_layout()
    timeline_plot_path = os.path.join(reports_dir, "decision_timeline.png")
    plt.savefig(timeline_plot_path, dpi=150)
    plt.close()
    print(f"Decision timeline plot saved to {timeline_plot_path}")
    
    print("\n=== ML Training & Evaluation Complete! ===")
    for m in summary["comparison_table"]:
        print(f"[{m['model_name']}] Accuracy: {m['accuracy']*100:.2f}%, Macro-F1: {m['macro_f1']:.3f}, Overexp Recall: {m['overexposure_recall']*100:.1f}%, False Alarm: {m['overexposure_false_alarm']*100:.1f}%")

if __name__ == "__main__":
    run_training_pipeline()
