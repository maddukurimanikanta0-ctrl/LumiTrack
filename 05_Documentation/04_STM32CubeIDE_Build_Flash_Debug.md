# STM32CubeIDE Build, Flash, and Debugging Guide

## Target Microcontroller: STM32F446RET6 (NUCLEO-F446RE)

This beginner-friendly guide walks through importing, building, flashing, and debugging the LumiTrack firmware using STM32CubeIDE.

---

## Step 1: Launch STM32CubeIDE & Select Workspace

1. Open **STM32CubeIDE** from the Windows Start Menu (or from `C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\stm32cubeide.exe`).
2. When prompted for the workspace directory:
   - Choose a convenient directory (e.g., `C:\Users\eswar\STM32CubeIDE\workspace_1.14` or leave default).
   - Click **Launch**.

---

## Step 2: Import the Completed Firmware Project

1. In the top menu bar, click **File** $\rightarrow$ **Open Projects from File System...**
2. In the dialog box:
   - Click the **Directory...** button next to *Import source*.
   - Navigate to the project firmware folder:  
     `C:\Users\eswar\OneDrive\Documents\ML_IOT\Project_details\Completed_Project\01_Firmware`
   - Click **Select Folder**.
3. In the project list below, you will see `LumiTrack_F446RE` checked as an Eclipse project.
4. Click **Finish**.
5. The project will now appear in the **Project Explorer** tree on the left panel.

---

## Step 3: Build the Firmware

1. Click on the project folder `LumiTrack_F446RE` in the Project Explorer to select it.
2. In the top toolbar, click the **Hammer icon** (Build), or press `Ctrl + B` (or menu **Project** $\rightarrow$ **Build Project**).
3. Observe the build output in the **Console** window at the bottom:
   ```text
   arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard ...
   Linking firmware ELF...
   Generating HEX and BIN files...
   === Firmware Build Success! ===
      text    data     bss     dec     hex filename
     22920     100    2488   25508    63a4 LumiTrack_F446RE.elf
   Build Finished. 0 errors, 0 warnings.
   ```
4. A binary target `LumiTrack_F446RE.elf` is generated in the `Build/` (or `Debug/`) directory.

---

## Step 4: Connect the NUCLEO-F446RE Board

1. Take a data-capable **USB-A to USB Mini-B (or Micro-B depending on cable/board revision)** cable.
2. Plug one end into your computer and the other end into the **ST-LINK USB connector** (at the top edge of the Nucleo board).
3. Observe board indicators:
   - **LD3 (PWR)**: Constant RED (board has 5V USB power).
   - **LD1 (COM)**: Red/Green flashing, then solid RED (ST-LINK ready).
4. Verify Windows Device Manager:
   - Open **Device Manager** (`Win + X` $\rightarrow$ Device Manager).
   - Expand **Ports (COM & LPT)**.
   - Look for: `STMicroelectronics STLink Virtual COM Port (COMx)` (e.g., `COM3`, `COM4`).
   - Note this COM port number—you will use it in VS Code!

---

## Step 5: Flash the Firmware onto the Board

1. In STM32CubeIDE, right-click `LumiTrack_F446RE` in the Project Explorer.
2. Select **Run As** $\rightarrow$ **STM32 C/C++ Application**.
3. If the *Edit Configuration* window appears:
   - Click the **Debugger** tab.
   - Ensure **Debug probe** is set to **ST-LINK (ST-LINK GDB server)**.
   - Under **Interface**, ensure **SWD** is selected.
   - Click **Apply**, then **OK**.
4. The IDE will connect to the ST-LINK, erase the flash memory, flash the program, and reset the MCU.
5. In the bottom Console, you will see:
   ```text
   Download verified successfully
   Shutting down...
   Starting target CPU...
   ```
6. **Physical Board Verification:**
   - Onboard green LED **LD2** will begin pulsing gently (1 Hz indicates `SAFE_HOME`).
   - If connected, the servo motor will sweep smoothly to the **90° neutral position**.

---

## Step 6: Debugging the Application

1. To run in interactive debug mode:
   - Right-click project $\rightarrow$ **Debug As** $\rightarrow$ **STM32 C/C++ Application**.
   - When asked to switch to the *Debug Perspective*, click **Switch**.
2. Execution will pause at the entry of `main()`.
3. In the top toolbar, you have:
   - **Resume (`F8`)**: Run continuously.
   - **Suspend**: Pause execution to inspect variables.
   - **Step Over (`F6`)**: Execute the current line without entering functions.
   - **Step Into (`F5`)**: Enter into the function call.
   - **Terminate (`Ctrl + F2`)**: Stop the debug session.
4. Set breakpoints by double-clicking in the left margin next to any line number (e.g. inside `Protocol_ReceiveByte()` or `ExecuteScanProfile()`).

---

## Step 7: Troubleshooting Common Errors

### Error 1: "ST-LINK firmware upgrade required"
- **Solution**: In the top menu, go to **Help** $\rightarrow$ **ST-LINK Firmware Upgrade** $\rightarrow$ Click **Device Connect** $\rightarrow$ Click **Upgrade**.

### Error 2: "Target not halting" or "No device found on target"
- **Cause**: Microcontroller is in low-power sleep or SWD pins were briefly reconfigured.
- **Solution**: 
  1. Hold down the black **USER RESET** button on the Nucleo board.
  2. Click **Run** or **Debug** in the IDE.
  3. As soon as the console says *"Connecting to target"*, release the black reset button.

### Error 3: "Virtual COM Port not showing in Device Manager"
- **Cause**: Missing ST-LINK USB driver.
- **Solution**: Run the ST-LINK driver installer located at `C:\Program Files (x86)\STMicroelectronics\stlink_server` or download `STSW-LINK009` from st.com.
