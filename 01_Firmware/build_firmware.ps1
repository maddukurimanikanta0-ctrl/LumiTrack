# PowerShell Build Script for LumiTrack NUCLEO-F446RE Firmware
# Uses the ST GNU ARM GCC Toolchain bundled with STM32CubeIDE

$ErrorActionPreference = "Stop"

$GccPath = "C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740\tools\bin"
if (-not (Test-Path "$GccPath\arm-none-eabi-gcc.exe")) {
    $GccCmd = Get-Command "arm-none-eabi-gcc.exe" -ErrorAction SilentlyContinue
    if ($GccCmd) {
        $GccPath = Split-Path $GccCmd.Source
    } else {
        Write-Error "arm-none-eabi-gcc.exe not found. Please install STM32CubeIDE or add ARM toolchain to PATH."
    }
}

$CC = "$GccPath\arm-none-eabi-gcc.exe"
$OBJCOPY = "$GccPath\arm-none-eabi-objcopy.exe"
$SIZE = "$GccPath\arm-none-eabi-size.exe"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

$OutDir = "$ScriptDir\Build"
if (-not (Test-Path $OutDir)) {
    New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
}

$CFlags = @(
    "-mcpu=cortex-m4",
    "-mthumb",
    "-mfpu=fpv4-sp-d16",
    "-mfloat-abi=hard",
    "-DSTM32F446xx",
    "-DUSE_HAL_DRIVER",
    "-O2",
    "-Wall",
    "-fdata-sections",
    "-ffunction-sections",
    "-I$ScriptDir\Core\Inc",
    "-I$ScriptDir\Drivers\CMSIS\Device\ST\STM32F4xx\Include",
    "-I$ScriptDir\Drivers\CMSIS\Include",
    "-I$ScriptDir\Drivers\STM32F4xx_HAL_Driver\Inc"
)

$Sources = @(
    "$ScriptDir\Core\Src\main.c",
    "$ScriptDir\Core\Src\safety.c",
    "$ScriptDir\Core\Src\servo.c",
    "$ScriptDir\Core\Src\sensors.c",
    "$ScriptDir\Core\Src\protocol.c",
    "$ScriptDir\Core\Src\stm32f4xx_it.c",
    "$ScriptDir\Core\Src\stm32f4xx_hal_msp.c",
    "$ScriptDir\Core\Src\system_stm32f4xx.c",
    "$ScriptDir\Core\Src\syscalls.c",
    "$ScriptDir\Core\Src\sysmem.c",
    "$ScriptDir\Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal.c",
    "$ScriptDir\Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_rcc.c",
    "$ScriptDir\Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_rcc_ex.c",
    "$ScriptDir\Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_gpio.c",
    "$ScriptDir\Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_dma.c",
    "$ScriptDir\Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_cortex.c",
    "$ScriptDir\Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_pwr.c",
    "$ScriptDir\Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_pwr_ex.c",
    "$ScriptDir\Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_flash.c",
    "$ScriptDir\Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_flash_ex.c",
    "$ScriptDir\Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_uart.c",
    "$ScriptDir\Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_tim.c",
    "$ScriptDir\Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_tim_ex.c",
    "$ScriptDir\Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_adc.c",
    "$ScriptDir\Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_adc_ex.c"
)

$Startup = "$ScriptDir\Core\Startup\startup_stm32f446retx.s"

Write-Host "Compiling C source files..." -ForegroundColor Cyan
$Objects = @()
foreach ($src in $Sources) {
    $objName = [System.IO.Path]::GetFileNameWithoutExtension($src) + ".o"
    $objPath = "$OutDir\$objName"
    $Objects += $objPath
    
    $args = $CFlags + @("-c", $src, "-o", $objPath)
    & $CC $args
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Compilation failed for $src"
    }
}

Write-Host "Assembling startup file..." -ForegroundColor Cyan
$StartupObj = "$OutDir\startup_stm32f446retx.o"
$Objects += $StartupObj
$AsmArgs = @(
    "-mcpu=cortex-m4",
    "-mthumb",
    "-mfpu=fpv4-sp-d16",
    "-mfloat-abi=hard",
    "-c", $Startup,
    "-o", $StartupObj
)
& $CC $AsmArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "Assembly failed for $Startup"
}

Write-Host "Linking firmware ELF..." -ForegroundColor Cyan
$ElfOut = "$OutDir\LumiTrack_F446RE.elf"
$MapOut = "$OutDir\LumiTrack_F446RE.map"
$LinkerScript = "$ScriptDir\STM32F446RETx_FLASH.ld"

$LinkArgs = @(
    "-mcpu=cortex-m4",
    "-mthumb",
    "-mfpu=fpv4-sp-d16",
    "-mfloat-abi=hard",
    "--specs=nano.specs",
    "--specs=nosys.specs",
    "-T$LinkerScript",
    "-Wl,-Map=$MapOut,--cref",
    "-Wl,--gc-sections"
) + $Objects + @("-lm", "-lc", "-o", $ElfOut)

& $CC $LinkArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "Linking failed"
}

Write-Host "Generating HEX and BIN files..." -ForegroundColor Cyan
$HexOut = "$OutDir\LumiTrack_F446RE.hex"
$BinOut = "$OutDir\LumiTrack_F446RE.bin"

& $OBJCOPY -O ihex $ElfOut $HexOut
& $OBJCOPY -O binary -S $ElfOut $BinOut

Write-Host "`n=== Firmware Build Success! ===" -ForegroundColor Green
& $SIZE $ElfOut
Write-Host "Target binaries generated in $OutDir`n" -ForegroundColor Green
