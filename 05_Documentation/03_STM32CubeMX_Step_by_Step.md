# STM32CubeMX Step-by-Step Configuration Guide

## Target Microcontroller: STM32F446RET6 (NUCLEO-F446RE)

This beginner-friendly guide walks through configuring the microcontroller hardware peripherals from scratch using the STM32CubeMX graphical interface.

---

## Step 1: Create the Project

1. Launch **STM32CubeMX** (or open the `.ioc` editor inside **STM32CubeIDE**).
2. Click **"Access to MCU Selector"** (or **"Access to Board Selector"**).
3. If using the Board Selector:
   - Type `NUCLEO-F446RE` in the search box.
   - Select the board and click **"Start Project"**.
   - When prompted: *"Initialize all peripherals with their default Mode?"*, click **No** (we will configure only the specific project pins).
4. If using the MCU Selector:
   - Type `STM32F446RETx` in the Part Number search box.
   - Double-click `STM32F446RETx` to open the Pinout view.

---

## Step 2: Configure System Core & Debug

1. In the left categories panel, expand **System Core**:
   - Click **SYS**:
     - **Debug**: Select **Serial Wire** (assigns `PA13` as `SWDIO` and `PA14` as `SWCLK`).
     - **Timebase Source**: Select **SysTick**.
   - Click **RCC**:
     - **High Speed Clock (HSE)**: Select **BYPASS Clock Source** (if using the 8 MHz clock supplied by the ST-LINK MCO), or leave disabled if using the internal 16 MHz HSI oscillator.

---

## Step 3: Configure GPIO Pins

1. On the graphical chip pinout:
   - Click pin **PA5**: Select **GPIO_Output** (User LED LD2).
     - Right-click pin PA5 $\rightarrow$ Enter User Label: `LD2_GREEN_LED`.
   - Click pin **PC13**: Select **GPIO_Input** (User Button B1).
     - Right-click pin PC13 $\rightarrow$ Enter User Label: `B1_USER_BUTTON`.
2. Under **System Core** $\rightarrow$ **GPIO**:
   - Select `PC13`: Set **GPIO Pull-up/Pull-down** to **Pull-up**.

---

## Step 4: Configure ADC1 (LDR and Potentiometer)

1. In the left panel, expand **Analog** $\rightarrow$ Click **ADC1**:
   - Check **IN0** (automatically assigns `PA0`).
   - Check **IN1** (automatically assigns `PA1`).
2. Under **Configuration** $\rightarrow$ **Parameter Settings**:
   - **Clock Prescaler**: `PCLK2 divided by 4` (22.5 MHz).
   - **Resolution**: `ADC 12-bit resolution`.
   - **Data Alignment**: `Right alignment`.
   - **Scan Conversion Mode**: `Disabled` (firmware configures and samples channels individually on demand).
   - **Continuous Conversion Mode**: `Disabled`.
   - **External Trigger Conversion Source**: `Regular Conversion launched by software`.
3. Label pins on the diagram:
   - Right-click `PA0` $\rightarrow$ User Label: `LDR_ADC_IN0`.
   - Right-click `PA1` $\rightarrow$ User Label: `POT_ADC_IN1`.

---

## Step 5: Configure TIM3 (50 Hz Servo PWM)

1. In the left panel, expand **Timers** $\rightarrow$ Click **TIM3**:
   - Set **Channel1** to **PWM Generation CH1** (assigns pin `PA6`).
2. Under **Configuration** $\rightarrow$ **Parameter Settings**:
   - **Prescaler (PSC)**: `89`  
     *(Since APB1 Timer clock is 90 MHz, Prescaler = 90 - 1 = 89 yields a 1.0 MHz timer clock: 1 tick = 1 microsecond)*.
   - **Counter Mode**: `Up`.
   - **Counter Period (AutoReload Register - ARR)**: `19999`  
     *(20000 ticks at 1.0 MHz = 20.0 ms period $\rightarrow$ exact 50 Hz PWM frequency)*.
   - **Internal Clock Division (CKD)**: `No Division`.
   - **Auto-Reload Preload**: `Enable`.
3. Under **PWM Generation Channel 1**:
   - **Mode**: `PWM mode 1`.
   - **Pulse (CCR1)**: `1500` *(Default 1.5 ms center pulse $\rightarrow$ 90° safe neutral)*.
   - **CH Polarity**: `High`.
4. Right-click pin `PA6` $\rightarrow$ User Label: `SERVO_PWM_TIM3_CH1`.

---

## Step 6: Configure USART2 (ST-LINK Virtual COM Port)

1. In the left panel, expand **Connectivity** $\rightarrow$ Click **USART2**:
   - Set **Mode** to **Asynchronous**.
   - Verify pin assignment: `PA2` is `USART2_TX`, `PA3` is `USART2_RX`.
2. Under **Configuration** $\rightarrow$ **Parameter Settings**:
   - **Baud Rate**: `115200 Bits/s`.
   - **Word Length**: `8 Bits (including Parity)`.
   - **Parity**: `None`.
   - **Stop Bits**: `1`.
   - **Data Direction**: `Receive and Transmit`.
   - **Over Sampling**: `16 Samples`.

---

## Step 7: Configure the Clock Tree (180 MHz System Clock)

1. Click the **Clock Configuration** tab at the top.
2. Select **HSI** (16 MHz) as the PLL source:
   - In the **PLL Source Mux**, select **HSI**.
   - Set **PLLM** division factor to `/ 16` (Input to VCO = 1 MHz).
   - Set **PLLN** multiplication factor to `* 360` (VCO Output = 360 MHz).
   - Set **PLLP** division factor to `/ 2` (SYSCLK = 180 MHz).
3. Set **System Clock Mux** to **PLLCLK**.
4. Configure bus dividers:
   - **AHB Prescaler**: `/ 1` $\rightarrow$ HCLK = 180 MHz.
   - **APB1 Prescaler**: `/ 4` $\rightarrow$ PCLK1 = 45 MHz (APB1 Timer Clocks = 90 MHz).
   - **APB2 Prescaler**: `/ 2` $\rightarrow$ PCLK2 = 90 MHz (APB2 Timer Clocks = 180 MHz).
5. Press **Enter**; CubeMX will automatically validate all frequencies.

---

## Step 8: Project Generation Settings

1. Click the **Project Manager** tab.
2. Under **Project**:
   - **Project Name**: `LumiTrack_F446RE`.
   - **Project Location**: Select `C:\Users\eswar\OneDrive\Documents\ML_IOT\Project_details\Completed_Project\01_Firmware`.
   - **Toolchain / IDE**: Select **STM32CubeIDE**.
3. Under **Code Generator**:
   - Select **"Copy only the necessary library files"**.
   - Check **"Generate peripheral initialization as a pair of '.c/.h' files per peripheral"** (keeps code organized).
   - Check **"Keep User Code when re-generating"** (essential so code between `/* USER CODE BEGIN */` and `/* USER CODE END */` is never deleted).
4. Click **"GENERATE CODE"** in the top-right corner.
