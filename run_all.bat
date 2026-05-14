@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

::==============================================================================
:: UFDE+ Yosys Flow - Batch Runner for All Configurations
:: Usage: run_all.bat [test_dir]
:: Example: run_all.bat
::          run_all.bat test
::==============================================================================

set "SCRIPT_DIR=%~dp0"
set "TEST_DIR=%~1"
if "%TEST_DIR%"=="" set "TEST_DIR=test"

:: Convert to absolute path
set "TEST_PATH=%SCRIPT_DIR%%TEST_DIR%"

if not exist "%TEST_PATH%" (
    echo Error: Test directory not found: %TEST_PATH%
    exit /b 1
)

echo ========================================
echo UFDE+ Yosys Flow - Batch Runner
echo Test Directory: %TEST_PATH%
echo ========================================
echo.

:: Count total configurations
set "TOTAL=0"
for /d %%d in ("%TEST_PATH%\*") do (
    if exist "%%d\top.v" (
        if exist "%%d\test.v" (
            if exist "%%d\top_cons.xml" (
                set /a TOTAL+=1
            )
        )
    )
)

if %TOTAL%==0 (
    echo Error: No valid configurations found in %TEST_PATH%
    echo Make sure each subdirectory contains: top.v, test.v, top_cons.xml
    exit /b 1
)

echo Found %TOTAL% configurations to process
echo.

:: Results are kept in each config's own directory
set "RESULTS_DIR=%TEST_PATH%\_results"
if exist "%RESULTS_DIR%" rmdir /s /q "%RESULTS_DIR%"
mkdir "%RESULTS_DIR%"

:: Process each configuration
set "PASSED=0"
set "FAILED=0"
set "CURRENT=0"

for /d %%d in ("%TEST_PATH%\*") do (
    set "CONFIG_NAME=%%~nxd"
    set "CONFIG_PATH=%%d"
    
    :: Check if it's a valid configuration
    if exist "%%d\top.v" if exist "%%d\test.v" if exist "%%d\top_cons.xml" (
        set /a CURRENT+=1
        echo.
        echo ========================================
        echo [!CURRENT!/%TOTAL%] Processing: !CONFIG_NAME!
        echo ========================================
        
        :: Run flow
        call "%SCRIPT_DIR%run_flow.bat" "%TEST_DIR%\!CONFIG_NAME!"
        
        if errorlevel 1 (
            echo [FAIL] !CONFIG_NAME!
            set /a FAILED+=1
            echo !CONFIG_NAME! >> "%RESULTS_DIR%\failed.txt"
        ) else (
            echo [PASS] !CONFIG_NAME!
            set /a PASSED+=1
            echo !CONFIG_NAME! >> "%RESULTS_DIR%\passed.txt"
        )
    )
)

:: Summary
echo.
echo ========================================
echo BATCH RUN COMPLETE
echo ========================================
echo Total:  %TOTAL%
echo Passed: %PASSED%
echo Failed: %FAILED%
echo.
echo Results saved to: %RESULTS_DIR%
echo   - passed.txt: List of successful configs
echo   - failed.txt: List of failed configs
echo.
echo Note: Each config's output files are kept in their own directory
echo ========================================

exit /b %FAILED%
