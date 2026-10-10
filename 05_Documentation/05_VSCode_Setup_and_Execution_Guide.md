# Visual Studio Code Setup and Execution Guide

This guide explains how to set up and run the Python Machine Learning pipeline, test suites, and IoT host applications inside Visual Studio Code.

---

## 1. Open the Project in VS Code

1. Launch **Visual Studio Code**.
2. Click **File** $\rightarrow$ **Open Folder...** (or press `Ctrl + K, Ctrl + O`).
3. Select the project folder:  
   `C:\Users\eswar\OneDrive\Documents\ML_IOT\Project_details\Completed_Project`
4. Click **Select Folder**.

---

## 2. Recommended VS Code Extensions

Open the Extensions view (`Ctrl + Shift + X`) and install the following recommended extensions:
1. **Python** (`ms-python.python`) – Official Python language support, debugging, and linting.
2. **Jupyter** (`ms-toolsai.jupyter`) – Interactive notebook support for `.ipynb` files.
3. **Serial Monitor** (`ms-vscode.serial-monitor`) – Built-in serial console for viewing STM32 UART logs directly in VS Code.

---

## 3. Select Python Interpreter

1. Open the Command Palette by pressing `Ctrl + Shift + P`.
2. Type and select: `Python: Select Interpreter`.
3. Choose the installed Python 3.11 runtime:  
   `Python 3.11.9 64-bit (C:\Users\eswar\AppData\Local\Python\pythoncore-3.11-64\python.exe)`
4. VS Code will now use Python 3.11 for all terminal runs, tests, and Jupyter notebooks.

---

## 4. Install Dependencies (Virtual Environment)

Open an integrated PowerShell terminal in VS Code (`Ctrl + ~` or menu **Terminal** $\rightarrow$ **New Terminal**):

```powershell
# Optional: Create and activate a dedicated virtual environment
py -3.11 -m venv .venv
.\.venv\Scripts\Activate.ps1

# Install required dependencies
py -3.11 -m pip install -r 03_IoT_Host_Application\requirements.txt
```

---

## 5. Running the Machine Learning Pipeline

### Step 5.1: Regenerate or Inspect Seed Dataset
```powershell
py -3.11 02_Machine_Learning\data\generate_dataset.py
```
*Generates `seed_scan_dataset.csv` with 600 trials across 15 sessions.*

### Step 5.2: Train Models & Generate Benchmark Evaluation
```powershell
py -3.11 02_Machine_Learning\src\train_models.py
```
*Trains Baseline Heuristic, Ridge Logistic Regression, Random Forest, and Partial-Scan Early Stopping Model.*  
*Generates confusion matrix plot (`confusion_matrix.png`), decision timeline plot (`decision_timeline.png`), and metrics summary (`metrics_summary.json`).*

### Step 5.3: Run Interactive Jupyter Notebook
1. In the VS Code file explorer, navigate to `02_Machine_Learning/notebooks/model_analysis.ipynb`.
2. Click to open the notebook.
3. In the top-right corner of the notebook editor, click **Select Kernel** $\rightarrow$ Choose **Python 3.11**.
4. Click **Run All** to execute all cells, display polar light-field profiles, confusion matrices, and interactive predictions.

---

## 6. Running the Automated Test Suite

Run the full verification suite covering protocol contract, ML feature extraction, inference, and closed-loop integration:

```powershell
py -3.11 -m unittest discover -s 04_Verification_and_Tests -p "test_*.py" -v
```

Expected Output:
```text
test_feature_extraction_structure ... ok
test_normal_diffuse_classification ... ok
test_overexposure_risk_classification ... ok
test_partial_scan_early_stopping ... ok
test_specular_confuser_classification ... ok
test_canonical_command_encoding ... ok
test_invalid_command_rejection ... ok
test_mcu_ack_nack_decoding ... ok
test_mcu_telemetry_decoding ... ok
test_closed_loop_scan_and_classification ... ok
test_initial_telemetry_and_state ... ok
test_manual_override_priority ... ok
test_watchdog_timeout_on_host_loss ... ok

----------------------------------------------------------------------
Ran 13 tests in 14.516s

OK
```

---

## 7. Running the IoT Host Application

The host application connects PC intelligence to the STM32 board and can be run in 4 different modes:

### Mode 1: Simulated Closed-Loop Verification (No Hardware Required)
Runs a complete automated closed-loop test with the built-in virtual STM32 NUCLEO-F446RE emulator:
```powershell
py -3.11 03_IoT_Host_Application\run_system.py --mode sim
```

### Mode 2: Live Terminal CLI Monitor
Displays an in-place ANSI terminal dashboard with live angle progress bars and cumulative lux-seconds gauges:
```powershell
py -3.11 03_IoT_Host_Application\run_system.py --mode monitor
```

### Mode 3: Interactive Streamlit Web Dashboard
Launches a rich browser interface with real-time polar light profiles and manual action dispatchers:
```powershell
py -3.11 -m streamlit run 03_IoT_Host_Application\dashboard_app.py
```
*The web dashboard opens automatically at `http://localhost:8501`.*

### Mode 4: Live Hardware Connection (With Physical Board)
1. Plug the STM32 NUCLEO-F446RE into your computer via USB.
2. Note the COM port from Windows Device Manager (e.g., `COM3`).
3. Launch the live controller:
```powershell
py -3.11 03_IoT_Host_Application\run_system.py --mode live --port COM3
```
*The host connects to the board at 115200 baud, reads live LDR/Potentiometer telemetry, runs ML inference, and commands protective servo angles.*
