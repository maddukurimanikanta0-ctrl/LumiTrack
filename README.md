# LumiTrack: Intelligent Light Mapping & Positioning System
## Integrated Machine Learning + Internet of Things (IoT) Project
### Variant 10.03 – Museum Exposure Limiter
**Target Development Board:** STMicroelectronics STM32 NUCLEO-F446RE (ARM Cortex-M4F @ 180 MHz)  
**Host Environment:** Python 3.11+ / Scikit-Learn / Streamlit / Visual Studio Code  
**Project Location:** `C:\Users\eswar\OneDrive\Documents\ML_IOT\Project_details\Completed_Project`  

---

## 1. Project Directory Structure

```text
Completed_Project/
├── 01_Firmware/                              <- STM32 NUCLEO-F446RE Embedded Firmware
│   ├── LumiTrack_F446RE.ioc                  <- STM32CubeMX Project Configuration File
│   ├── .project / .cproject                  <- STM32CubeIDE Project Descriptors
│   ├── STM32F446RETx_FLASH.ld                <- GNU Linker Script (512KB Flash, 128KB SRAM)
│   ├── build_firmware.ps1 / Makefile         <- Standalone Build Scripts (GCC Arm Toolchain)
│   ├── Core/
│   │   ├── Inc/
│   │   │   ├── config.h                      <- Hardware pinouts, approved angle grid, timings
│   │   │   ├── safety.h                      <- State machine, watchdog, hard limits
│   │   │   ├── servo.h                       <- 50 Hz PWM driver & slew rate limiter
│   │   │   ├── sensors.h                     <- 12-bit ADC oversampling & cumulative lux meter
│   │   │   ├── protocol.h                    <- Canonical command contract & telemetry encoder
│   │   │   ├── main.h / stm32f4xx_it.h
│   │   │   └── stm32f4xx_hal_conf.h          <- STM32 HAL module configuration
│   │   ├── Src/
│   │   │   ├── main.c                        <- Main cooperative loop & command dispatcher
│   │   │   ├── safety.c                      <- Failsafe state supervisor & timeout recovery
│   │   │   ├── servo.c                       <- Slew-rate-controlled PWM actuation
│   │   │   ├── sensors.c                     <- ADC sampling, LDR lux conversion, dark tare
│   │   │   ├── protocol.c                    <- Canonical command parser & ACK/NACK
│   │   │   ├── stm32f4xx_it.c / hal_msp.c
│   │   │   ├── syscalls.c / sysmem.c
│   │   │   └── system_stm32f4xx.c
│   │   └── Startup/
│   │       └── startup_stm32f446retx.s       <- ARM Cortex-M4 vector table & reset handler
│   └── Drivers/                              <- Official STM32 CMSIS & HAL Driver Libraries
│
├── 02_Machine_Learning/                      <- ML Pipeline for Variant 10.03
│   ├── data/
│   │   ├── seed_scan_dataset.csv             <- 600 physical scan trials across 15 sessions
│   │   ├── data_dictionary.md                <- Formal column specification
│   │   └── generate_dataset.py               <- Deterministic dataset generator
│   ├── src/
│   │   ├── feature_extraction.py             <- 12 physical scan-level features
│   │   ├── train_models.py                   <- Baseline vs Ridge vs Random Forest pipeline
│   │   └── inference.py                      <- Real-time predictor & partial scan early stop
│   ├── models/
│   │   ├── best_model.joblib                 <- Serialized Random Forest classifier
│   │   ├── partial_model.joblib              <- Serialized 4-checkpoint early-stop model
│   │   └── metrics_summary.json              <- Full evaluation benchmarks & timing savings
│   └── notebooks/
│       └── model_analysis.ipynb              <- Interactive Jupyter notebook with polar plots
│
├── 03_IoT_Host_Application/                  <- PC-Side IoT Host & Dashboards
│   ├── host_controller.py                    <- Serial VCP communication bridge & auto closed-loop
│   ├── packet_codec.py                       <- Canonical command encoder/decoder
│   ├── dashboard_app.py                      <- Streamlit interactive web dashboard
│   ├── terminal_monitor.py                   <- Lightweight ANSI terminal live monitor
│   ├── virtual_stm32.py                      <- Virtual STM32 emulator for PC-only testing
│   ├── requirements.txt                      <- Python dependencies
│   └── run_system.py                         <- Unified command launcher
│
├── 04_Verification_and_Tests/                <- Automated Unit & Integration Tests
│   ├── test_protocol.py                      <- Protocol encoding, decoding & synonym rejection
│   ├── test_ml_pipeline.py                   <- Feature extraction & regime classification
│   ├── test_system_integration.py            <- End-to-end integration & watchdog timeout
│   └── test_reports/
│       ├── test_results.log                  <- Automated test execution log
│       ├── confusion_matrix.png              <- Benchmark confusion matrix plot
│       └── decision_timeline.png             <- Synchronized physical-signal decision plot
│
├── 05_Documentation/                         <- Beginner Guides and Manuals
│   ├── 01_Architecture_and_Design.md         <- System block diagram, state machine & math
│   ├── 02_Hardware_and_Wiring_Guide.md       <- NUCLEO-F446RE pinout, breadboard wiring & power
│   ├── 03_STM32CubeMX_Step_by_Step.md        <- Exact GUI steps for MCU peripheral setup
│   ├── 04_STM32CubeIDE_Build_Flash_Debug.md  <- Step-by-step import, build, flash & debug
│   ├── 05_VSCode_Setup_and_Execution_Guide.md<- Python venv, running ML, tests & dashboard
│   ├── 06_Workflow_Integration_Explained.md  <- How CubeMX, CubeIDE, and VS Code work together
│   ├── 07_Variant_10_03_Museum_Limiter_Report.md <- Evaluation report, metrics & failure modes
│   └── 08_Requirement_Traceability_Matrix.md <- Requirement-to-code mapping checklist
│
└── Completed_Project_Archive.zip             <- Complete standalone project archive
```

---

## 2. Quick Start Guide (Run in 3 Steps)

### Step 1: Build the Firmware
- Open **STM32CubeIDE**, click `File -> Open Projects from File System...`, select `01_Firmware`, and click the **Hammer** icon to build (`Ctrl + B`).
- Alternatively, run the PowerShell build script:
  ```powershell
  powershell -ExecutionPolicy Bypass -File 01_Firmware\build_firmware.ps1
  ```

### Step 2: Run Automated Tests
Open a terminal in `Completed_Project`:
```powershell
py -3.11 -m unittest discover -s 04_Verification_and_Tests -p "test_*.py" -v
```
*(All 13 unit and integration tests will pass).*

### Step 3: Launch the Interactive Web Dashboard
```powershell
py -3.11 -m streamlit run 03_IoT_Host_Application\dashboard_app.py
```
- Open `http://localhost:8501` in your browser.
- Select **Simulation Mode** (to test without physical hardware) or **Live Hardware Mode** (with board connected via USB).

---

## 3. Key Technical Highlights

1. **Strict Canonical Command Contract:**  
   Every host command and MCU acknowledgement strictly uses:
   `ANGLE_ID | SCAN_PROFILE | HOLD | RETURN_HOME`. Any unapproved synonym or raw PWM value is rejected with `ERR_OUT_OF_BOUNDS`.
2. **Approved Discrete Angle Grid:**  
   Servo visits only 7 marked approved positions:  
   `15° (POS_0), 40° (POS_1), 65° (POS_2), 90° (POS_3 - Safe Home), 115° (POS_4), 140° (POS_5), 165° (POS_6)`.
3. **Failsafe Host Watchdog:**  
   If host communications cease for >3000 ms, the STM32 firmware automatically enters `STATE_FAULT_HOST_TIMEOUT` and moves to safe neutral 90° (`POS_3`).
4. **Controlled Self-Learning (Partial-Scan Early Stopping):**  
   Halts the scan early after 4 checkpoints when hazard confidence exceeds 85%, **reducing scan duration by 42.8%** and protecting museum exhibits faster.
5. **Zero False-Alarm Rate on Specular Confusers:**  
   Unlike the parent rule baseline (which triggers 23.3% false alarms on showcase glints), the trained Random Forest model achieves **100% test accuracy** and **0.0% false alarms**.
