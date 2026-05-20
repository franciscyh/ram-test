@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

::==============================================================================
:: RAM Hardware Test Runner
:: Usage: run_hw_test.bat <config_dir> [options]
:: Example: run_hw_test.bat test\4x2048
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

:: Pass all arguments to hw_test.exe
"%HW_TEST_EXE%" %*

exit /b %errorlevel%
