@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

::==============================================================================
:: Quick ModelSim Simulation Runner (no bitstream check)
:: Usage: run_sim_quick.bat <config_dir> [modelsim_dir]
::   modelsim_dir: Path to ModelSim/QuestaSim installation (default: D:\ModelSim\win64)
:: Example:
::   run_sim_quick.bat test\16x1024
::   run_sim_quick.bat test\16x1024 "C:\QuestaSim\win64"
::==============================================================================

set "SCRIPT_DIR=%~dp0"
set "CONFIG_DIR=%~1"
set "MODELSIM_DIR=%~2"
if "%MODELSIM_DIR%"=="" set "MODELSIM_DIR=D:\ModelSim\win64"

if "%CONFIG_DIR%"=="" (
    echo Usage: run_sim_quick.bat ^<config_dir^>
    exit /b 1
)

set "CONFIG_PATH=%~f1"
if not exist "%CONFIG_PATH%" (
    echo Error: Directory not found: %CONFIG_PATH%
    exit /b 1
)

for %%f in ("%CONFIG_PATH%") do set "CONFIG_NAME=%%~nxf"

if not exist "%CONFIG_PATH%\top.v" (
    echo Error: top.v not found
    exit /b 1
)
if not exist "%CONFIG_PATH%\test.v" (
    echo Error: test.v not found
    exit /b 1
)
if not exist "%CONFIG_PATH%\tb_test.sv" (
    echo Error: tb_test.sv not found
    exit /b 1
)

echo ========================================
echo Quick Simulation: %CONFIG_NAME%
echo ========================================

set "WORK_DIR=%CONFIG_PATH%\modelsim_work"
if exist "%WORK_DIR%" rmdir /s /q "%WORK_DIR%"
mkdir "%WORK_DIR%"
cd /d "%WORK_DIR%"

echo [0/3] Previous simulation results cleaned

:: Create work library
if exist "work" rmdir /s /q work
"%MODELSIM_DIR%\vlib" work > vlib.log 2>&1
if errorlevel 1 (
    echo Error: vlib failed
    type vlib.log
    exit /b 1
)
echo [1/3] Work library created

:: Compile
set "CUSTOM_SIMLIB=%SCRIPT_DIR%bin\resource\edalib\custom_simlib.v"
"%MODELSIM_DIR%\vlog" "%CUSTOM_SIMLIB%" "%CONFIG_PATH%\test.v" "%CONFIG_PATH%\top.v" "%CONFIG_PATH%\tb_test.sv" > vlog.log 2>&1
if errorlevel 1 (
    echo Error: Compilation failed
    type vlog.log | findstr /i "error"
    exit /b 1
)
echo [2/3] Compilation successful

:: Run simulation
"%MODELSIM_DIR%\vsim" -c -novopt tb_%CONFIG_NAME% -do "run -all; quit -f" > vsim.log 2>&1
if errorlevel 1 (
    echo Error: Simulation failed
    type vsim.log | findstr /i "error"
    exit /b 1
)
echo [3/3] Simulation completed

:: Check results
echo.
echo Results:
type vsim.log | findstr /c:"RESULT:"
type vsim.log | findstr /c:"Total checks:"
type vsim.log | findstr /c:"Total errors:"

findstr /c:"RESULT: PASSED" vsim.log >nul 2>&1
if %errorlevel% == 0 exit /b 0
exit /b 1
