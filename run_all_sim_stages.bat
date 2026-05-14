@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

::==============================================================================
:: Multi-Stage ModelSim Batch Simulation Runner
:: Usage: run_all_sim_stages.bat [test_dir] [stages] [modelsim_dir]
::   stages: comma-separated list of rtl,map,pack,route (default: all)
::   modelsim_dir: Path to ModelSim/QuestaSim installation (default: D:\ModelSim\win64)
:: Examples:
::   run_all_sim_stages.bat test rtl,map                        - Run RTL and Map stages
::   run_all_sim_stages.bat test route                          - Run only Route stage
::   run_all_sim_stages.bat test rtl,map "C:\QuestaSim\win64"   - Run with custom ModelSim path
::==============================================================================

set "SCRIPT_DIR=%~dp0"
set "TEST_DIR=%~1"
set "STAGES=%~2"
set "MODELSIM_DIR=%~3"

if "%TEST_DIR%"=="" set "TEST_DIR=test"
if "%STAGES%"=="" set "STAGES=rtl,map,pack,route"

set "TEST_PATH=%SCRIPT_DIR%%TEST_DIR%"

if not exist "%TEST_PATH%" (
    echo Error: Test directory not found: %TEST_PATH%
    exit /b 1
)

:: Parse stages
set "RUN_RTL=0"
set "RUN_MAP=0"
set "RUN_PACK=0"
set "RUN_ROUTE=0"

echo %STAGES% | findstr /i "rtl" >nul && set "RUN_RTL=1"
echo %STAGES% | findstr /i "map" >nul && set "RUN_MAP=1"
echo %STAGES% | findstr /i "pack" >nul && set "RUN_PACK=1"
echo %STAGES% | findstr /i "route" >nul && set "RUN_ROUTE=1"

echo ========================================
echo Multi-Stage ModelSim Batch Simulation
echo Test Directory: %TEST_PATH%
echo Stages: %STAGES%
echo ========================================
echo.
echo Note: Only configs with 'top_yosys_bit.bit' (run_flow.bat passed) will be simulated.
echo.

:: Create results directory
set "RESULTS_DIR=%TEST_PATH%\_sim_stage_results"
if exist "%RESULTS_DIR%" rmdir /s /q "%RESULTS_DIR%"
mkdir "%RESULTS_DIR%"

:: Build list of valid configs (excluding result directories and checking bitstream)
set "VALID_CONFIGS="
set "TOTAL=0"
set "WITH_BITSTREAM=0"

for /d %%d in ("%TEST_PATH%\*") do (
    set "DIR_NAME=%%~nxd"
    
    :: Skip result directories (check first char is not underscore)
    set "FIRST_CHAR=!DIR_NAME:~0,1!"
    if not "!FIRST_CHAR!"=="_" (
        set /a TOTAL+=1
        if exist "%%d\top_yosys_bit.bit" (
            set /a WITH_BITSTREAM+=1
            set "VALID_CONFIGS=!VALID_CONFIGS! %%d"
        )
    )
)

echo Total configs: %TOTAL%
echo With bitstream (run_flow passed): %WITH_BITSTREAM%
echo Skipped (flow failed or result dir): %TOTAL% - %WITH_BITSTREAM%
echo.

if %WITH_BITSTREAM%==0 (
    echo No configs with bitstream found. Please run run_flow.bat first:
    echo   run_all.bat
    exit /b 1
)

:: Process each stage
set "OVERALL_FAILED=0"

if "%RUN_RTL%"=="1" call :run_stage rtl
if "%RUN_MAP%"=="1" call :run_stage map
if "%RUN_PACK%"=="1" call :run_stage pack
if "%RUN_ROUTE%"=="1" call :run_stage route

:: Summary
echo.
echo ========================================
echo ALL STAGES COMPLETE
echo ========================================
echo Results saved to: %RESULTS_DIR%

for %%s in (rtl map pack route) do (
    if exist "%RESULTS_DIR%\%%s_summary.txt" (
        echo.
        echo [%%s stage]
        type "%RESULTS_DIR%\%%s_summary.txt"
    )
)

if %OVERALL_FAILED% gtr 0 (
    echo.
    echo Some simulations failed!
    exit /b 1
)

exit /b 0

::==============================================================================
:: Subroutine: run_stage
::==============================================================================
:run_stage
set "STAGE=%~1"
echo.
echo ========================================
echo Running %STAGE% stage simulation
echo ========================================

set "STAGE_PASSED=0"
set "STAGE_FAILED=0"
set "STAGE_SKIPPED_FLOW=0"
set "STAGE_SKIPPED_NETLIST=0"
set "STAGE_SKIPPED_PREV=0"
set "STAGE_CURRENT=0"

:: Determine previous stage failed file (if any)
set "PREV_FAILED_FILE="
if /i "%STAGE%"=="pack" set "PREV_FAILED_FILE=%RESULTS_DIR%\map_failed.txt"
if /i "%STAGE%"=="route" set "PREV_FAILED_FILE=%RESULTS_DIR%\pack_failed.txt"

:: Process only valid configs
for %%d in (%VALID_CONFIGS%) do (
    set "CONFIG_NAME=%%~nxd"
    set /a STAGE_CURRENT+=1
    
    echo.
    echo [!STAGE_CURRENT!/%WITH_BITSTREAM%] %STAGE%: !CONFIG_NAME!
    
    :: Check if netlist exists for this stage
    set "NETLIST_EXISTS=0"
    if /i "%STAGE%"=="rtl" set "NETLIST_EXISTS=1"
    if /i "%STAGE%"=="map" (
        if exist "%%d\top_yosys_map.v" set "NETLIST_EXISTS=1"
        if exist "%%d\!CONFIG_NAME!_map.v" set "NETLIST_EXISTS=1"
        if exist "%%d\!CONFIG_NAME!_gate.v" set "NETLIST_EXISTS=1"
    )
    if /i "%STAGE%"=="pack" (
        if exist "%%d\top_yosys_pack.v" set "NETLIST_EXISTS=1"
        if exist "%%d\!CONFIG_NAME!_pack.v" set "NETLIST_EXISTS=1"
    )
    if /i "%STAGE%"=="route" (
        if exist "%%d\top_yosys_route.v" set "NETLIST_EXISTS=1"
        if exist "%%d\!CONFIG_NAME!_route.v" set "NETLIST_EXISTS=1"
    )
    
    :: Check if previous stage failed for this config
    set "PREV_FAILED=0"
    if not "!PREV_FAILED_FILE!"=="" (
        if exist "!PREV_FAILED_FILE!" (
            findstr /i /c:"!CONFIG_NAME!" "!PREV_FAILED_FILE!" >nul 2>&1
            if !errorlevel! == 0 set "PREV_FAILED=1"
        )
    )
    
    if "!PREV_FAILED!"=="1" (
        echo   [SKIP] !CONFIG_NAME! - Previous stage failed
        set /a STAGE_SKIPPED_PREV+=1
        echo !CONFIG_NAME! >> "%RESULTS_DIR%\%STAGE%_skipped_prev.txt"
    ) else if "!NETLIST_EXISTS!"=="1" (
        call "%SCRIPT_DIR%run_sim_stage.bat" "%%d" %STAGE% "%MODELSIM_DIR%"
        
        if !errorlevel! neq 0 (
            echo   [FAIL] !CONFIG_NAME!
            set /a STAGE_FAILED+=1
            set /a OVERALL_FAILED+=1
            echo !CONFIG_NAME! >> "%RESULTS_DIR%\%STAGE%_failed.txt"
        ) else (
            echo   [PASS] !CONFIG_NAME!
            set /a STAGE_PASSED+=1
            echo !CONFIG_NAME! >> "%RESULTS_DIR%\%STAGE%_passed.txt"
        )
    ) else (
        echo   [SKIP] !CONFIG_NAME! - No netlist for %STAGE% stage
        set /a STAGE_SKIPPED_NETLIST+=1
        echo !CONFIG_NAME! >> "%RESULTS_DIR%\%STAGE%_skipped_no_netlist.txt"
    )
)

:: Save stage summary
echo Passed: %STAGE_PASSED% > "%RESULTS_DIR%\%STAGE%_summary.txt"
echo Failed: %STAGE_FAILED% >> "%RESULTS_DIR%\%STAGE%_summary.txt"
echo Skipped (previous stage failed): %STAGE_SKIPPED_PREV% >> "%RESULTS_DIR%\%STAGE%_summary.txt"
echo Skipped (no netlist): %STAGE_SKIPPED_NETLIST% >> "%RESULTS_DIR%\%STAGE%_summary.txt"
echo Total: %STAGE_CURRENT% >> "%RESULTS_DIR%\%STAGE%_summary.txt"

goto :eof
