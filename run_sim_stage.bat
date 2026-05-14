@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

::==============================================================================
:: ModelSim Simulation Runner for Different Stages
:: Usage: run_sim_stage.bat <config_dir> [stage] [modelsim_dir]
::   stage: rtl | map | pack | route (default: rtl)
::   modelsim_dir: Path to ModelSim/QuestaSim installation (default: D:\ModelSim\win64)
:: Examples:
::   run_sim_stage.bat test\16x1024 rtl                           - RTL simulation
::   run_sim_stage.bat test\16x1024 map                           - Post-mapping simulation
::   run_sim_stage.bat test\16x1024 rtl "C:\QuestaSim\win64"      - RTL with custom ModelSim path
::==============================================================================

set "SCRIPT_DIR=%~dp0"
set "CONFIG_PATH=%~f1"
set "STAGE=%~2"
set "MODELSIM_DIR=%~3"
if "%MODELSIM_DIR%"=="" set "MODELSIM_DIR=D:\ModelSim\win64"
set "EDALIB_DIR=%SCRIPT_DIR%bin\resource\edalib"

if "%CONFIG_PATH%"=="" (
    echo Usage: run_sim_stage.bat ^<config_dir^> [stage]
    echo   stage: rtl ^| map ^| pack ^| route
    exit /b 1
)

if "%STAGE%"=="" set "STAGE=rtl"

:: Verify stage
if /i not "%STAGE%"=="rtl" if /i not "%STAGE%"=="map" if /i not "%STAGE%"=="pack" if /i not "%STAGE%"=="route" (
    echo Error: Invalid stage '%STAGE%'. Must be: rtl, map, pack, or route
    exit /b 1
)

:: Extract config name
for %%f in ("%CONFIG_PATH%") do set "CONFIG_NAME=%%~nxf"

echo ========================================
echo ModelSim Simulation - Stage: %STAGE%
echo Config: %CONFIG_NAME%
echo ========================================

::==============================================================================
:: Determine netlist and simlib based on stage
::==============================================================================
set "NETLIST_FILE="
set "SIMLIB_FILE="
set "TOP_MODULE=tb_%CONFIG_NAME%"
set "NEEDS_PREPROCESS=0"

if /i "%STAGE%"=="rtl" (
    set "NETLIST_FILE=test.v"
    set "SIMLIB_FILE=custom_simlib.v"
    echo [INFO] RTL simulation using test.v
)

if /i "%STAGE%"=="map" (
    :: Try different naming conventions
    if exist "%CONFIG_PATH%\top_yosys_map.v" (
        set "NETLIST_FILE=top_yosys_map.v"
        set "NEEDS_PREPROCESS=1"
    ) else if exist "%CONFIG_PATH%\%CONFIG_NAME%_map.v" (
        set "NETLIST_FILE=%CONFIG_NAME%_map.v"
        set "NEEDS_PREPROCESS=1"
    ) else if exist "%CONFIG_PATH%\%CONFIG_NAME%_gate.v" (
        set "NETLIST_FILE=%CONFIG_NAME%_gate.v"
        set "NEEDS_PREPROCESS=1"
    )
    set "SIMLIB_FILE=MAPPING_SIMLIB.v"
    echo [INFO] Post-mapping simulation (preprocessing required)
)

if /i "%STAGE%"=="pack" (
    if exist "%CONFIG_PATH%\top_yosys_pack.v" (
        set "NETLIST_FILE=top_yosys_pack.v"
    ) else (
        set "NETLIST_FILE=%CONFIG_NAME%_pack.v"
    )
    set "SIMLIB_FILE=PACKING_SIMLIB_FDP3P7.v"
    set "NEEDS_PREPROCESS=1"
    echo [INFO] Post-packing simulation (preprocessing required)
)

if /i "%STAGE%"=="route" (
    if exist "%CONFIG_PATH%\top_yosys_route.v" (
        set "NETLIST_FILE=top_yosys_route.v"
    ) else (
        set "NETLIST_FILE=%CONFIG_NAME%_route.v"
    )
    set "SIMLIB_FILE=ROUTING_SIMLIB_FDP3P7.v"
    set "NEEDS_PREPROCESS=1"
    echo [INFO] Post-routing simulation (preprocessing required)
)

:: Check netlist exists
if not exist "%CONFIG_PATH%\%NETLIST_FILE%" (
    echo Error: Netlist not found: %CONFIG_PATH%\%NETLIST_FILE%
    exit /b 1
)

:: Check simlib exists
if not exist "%EDALIB_DIR%\%SIMLIB_FILE%" (
    echo Error: Simulation library not found: %EDALIB_DIR%\%SIMLIB_FILE%
    exit /b 1
)

:: Check testbench exists
if not exist "%CONFIG_PATH%\tb_test.sv" (
    echo Error: Testbench not found: %CONFIG_PATH%\tb_test.sv
    exit /b 1
)

:: Check wrapper exists (only for RTL stage)
if /i "%STAGE%"=="rtl" (
    if not exist "%CONFIG_PATH%\top.v" (
        echo Error: Wrapper not found: %CONFIG_PATH%\top.v
        exit /b 1
    )
)

::==============================================================================
:: Setup work directory
::==============================================================================
set "WORK_DIR=%CONFIG_PATH%\modelsim_work_%STAGE%"
if exist "%WORK_DIR%" rmdir /s /q "%WORK_DIR%"
mkdir "%WORK_DIR%"
cd /d "%WORK_DIR%"

:: Create do file
set "DO_FILE=sim_run.do"
echo vcd file waveform_%STAGE%.vcd > %DO_FILE%
echo vcd add -r /* >> %DO_FILE%
echo run -all >> %DO_FILE%
echo quit -f >> %DO_FILE%

::==============================================================================
:: Step 1: Create work library
::==============================================================================
echo.
echo [Step 1/4] Creating work library...

"%MODELSIM_DIR%\vlib" work > vlib.log 2>&1
if errorlevel 1 (
    echo Error: vlib failed
    type vlib.log
    exit /b 1
)
echo   OK: work library created

::==============================================================================
:: Step 2: Preprocess netlist (for map/pack/route stages)
::==============================================================================
set "NETLIST_TO_COMPILE=%CONFIG_PATH%\%NETLIST_FILE%"

if "%NEEDS_PREPROCESS%"=="1" (
    echo.
    echo [Step 2/4] Preprocessing netlist for %STAGE% stage...
    
    :: Preprocess: fix netlist format issues
    set "PROCESSED_NETLIST=%WORK_DIR%\processed.v"
    
    :: Use PowerShell script for regex processing
    :: Note: Use delayed expansion for variables set inside if block
    powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%scripts\preprocess_netlist.ps1" -InputFile "%CONFIG_PATH%\%NETLIST_FILE%" -OutputFile "!PROCESSED_NETLIST!"
    
    if errorlevel 1 (
        echo Error: Preprocessing failed
        exit /b 1
    )
    
    set "NETLIST_TO_COMPILE=!PROCESSED_NETLIST!"
    echo   OK: Netlist preprocessed
) else (
    echo.
    echo [Step 2/4] Skipping preprocessing (not needed for %STAGE% stage)
)

::==============================================================================
:: Step 3: Compile
::==============================================================================
echo.
echo [Step 3/4] Compiling...
echo   Stage simlib: %SIMLIB_FILE%
echo   Base simlib: custom_simlib.v
echo   Netlist: %NETLIST_FILE%

set "COMPILE_CMD=%MODELSIM_DIR%\vlog"

:: Compile: simlib + netlist + testbench
:: Note: For RTL stage we load custom_simlib.v (for RAMB4 primitives).
:: For post-synthesis stages, we only load custom_simlib.v because it contains
:: the BLOCKRAM behavioral model. The stage-specific simlibs (PACKING/ROUTING)
:: are kept for the pack/route tools and are NOT loaded into ModelSim.
if /i "%STAGE%"=="rtl" (
    :: RTL: custom_simlib + netlist + wrapper + testbench
    %COMPILE_CMD% "%EDALIB_DIR%\custom_simlib.v" "%NETLIST_TO_COMPILE%" "%CONFIG_PATH%\top.v" "%CONFIG_PATH%\tb_test.sv" > vlog.log 2>&1
) else (
    :: Post-synthesis: custom_simlib + stage simlib + netlist + testbench (no wrapper)
    %COMPILE_CMD% "%EDALIB_DIR%\custom_simlib.v" "%EDALIB_DIR%\%SIMLIB_FILE%" "%NETLIST_TO_COMPILE%" "%CONFIG_PATH%\tb_test.sv" > vlog.log 2>&1
)

if errorlevel 1 (
    echo Error: Compilation failed
    type vlog.log | findstr /i "error"
    exit /b 1
)
echo   OK: Compilation successful

::==============================================================================
:: Step 4: Run simulation
::==============================================================================
echo.
echo [Step 4/4] Running simulation...

"%MODELSIM_DIR%\vsim" -c -novopt %TOP_MODULE% -do %DO_FILE% > vsim.log 2>&1

if errorlevel 1 (
    echo Error: Simulation failed
    type vsim.log | findstr /i "error"
    exit /b 1
)

::==============================================================================
:: Check results
::==============================================================================
echo.
echo Results:
type vsim.log | findstr /c:"RESULT:"
type vsim.log | findstr /c:"Total checks:"
type vsim.log | findstr /c:"Total errors:"

findstr /c:"RESULT: PASSED" vsim.log >nul 2>&1
if %errorlevel% == 0 (
    echo.
    echo [PASS] %STAGE% simulation passed for %CONFIG_NAME%
    exit /b 0
) else (
    echo.
    echo [FAIL] %STAGE% simulation failed for %CONFIG_NAME%
    exit /b 1
)
