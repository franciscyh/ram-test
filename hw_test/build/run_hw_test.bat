@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

::==============================================================================
:: RAM Hardware Test Runner
:: Usage: run_hw_test.bat <config_dir> [options]
:: Example: run_hw_test.bat test\4x2048
::          run_hw_test.bat test\4x2048_4x2048
::          run_hw_test.bat test\4x2048_8x1024 -d
::          run_hw_test.bat test\4x2048 -f 100 -t 1
::==============================================================================

set "SCRIPT_DIR=%~dp0"
set "BUILD_DIR=%SCRIPT_DIR%build"
set "HW_TEST_EXE=%BUILD_DIR%\hw_test.exe"

:: Check if hw_test.exe exists, build if not
if not exist "%HW_TEST_EXE%" (
    echo [INFO] hw_test.exe not found, building...
    call "%SCRIPT_DIR%build.bat"
    if errorlevel 1 (
        echo [ERROR] Build failed
        exit /b 1
    )
)

:: Check again after build
if not exist "%HW_TEST_EXE%" (
    echo [ERROR] hw_test.exe still not found after build
    exit /b 1
)

:: Parse arguments to auto-detect dual-port config
:: If config dir name contains '_' and user did not specify -d, auto-enable it
set "CONFIG_DIR="
set "HAS_DUAL_FLAG=0"
set "ALL_ARGS=%*"

for %%a in (%*) do (
    set "ARG=%%a"
    if "!ARG:~0,1!"=="-" (
        if "!ARG!"=="-d" set HAS_DUAL_FLAG=1
        if "!ARG!"=="--dual-port" set HAS_DUAL_FLAG=1
    ) else if "!CONFIG_DIR!"=="" (
        set "CONFIG_DIR=!ARG!"
    )
)

set "AUTO_DUAL="
if !HAS_DUAL_FLAG!==0 (
    if not "!CONFIG_DIR!"=="" (
        :: Extract directory name (last component)
        for %%f in ("!CONFIG_DIR!") do set "DIR_NAME=%%~nxf"
        echo !DIR_NAME! | findstr /C:"_" >nul
        if !errorlevel!==0 (
            set "AUTO_DUAL=-d"
            echo [INFO] Auto-detected dual-port config: !DIR_NAME!, enabling -d
        )
    )
)

:: Pass all arguments to hw_test.exe, append -d if auto-detected
if "!AUTO_DUAL!"=="" (
    "%HW_TEST_EXE%" %*
) else (
    "%HW_TEST_EXE%" %* !AUTO_DUAL!
)

exit /b %errorlevel%
