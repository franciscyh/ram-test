@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
set "BUILD_DIR=%SCRIPT_DIR%build"
set "VLFD_FFI_DIR=E:\Rabbit\vlfd-ffi\target\release"
set "VLFD_FFI_INC=E:\Rabbit\vlfd-ffi"

if not exist "%VLFD_FFI_DIR%\vlfd_ffi.lib" (
    echo [ERROR] vlfd_ffi.lib not found: %VLFD_FFI_DIR%\vlfd_ffi.lib
    echo         Please build vlfd-ffi first:
    echo         cd E:\Rabbit\vlfd-ffi ^&^& cargo build --release
    exit /b 1
)

if not exist "%VLFD_FFI_DIR%\vlfd_ffi.dll" (
    echo [ERROR] vlfd_ffi.dll not found: %VLFD_FFI_DIR%\vlfd_ffi.dll
    exit /b 1
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

:: Try MSVC first
if exist "D:\MicrosoftVisualStudio\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "D:\MicrosoftVisualStudio\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul
    goto :build_msvc
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul
    goto :build_msvc
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul
    goto :build_msvc
)

:: Fallback to MinGW
where g++ >nul 2>nul
if %errorlevel% == 0 (
    goto :build_gcc
)

echo [ERROR] No C++ compiler found. Please install Visual Studio or MinGW.
exit /b 1

:build_msvc
echo [BUILD] Using MSVC compiler...

echo [BUILD] Compiling source files...

:: NOTE: Do NOT quote paths ending with backslash (\) - the trailing \" is
:: interpreted as an escaped quote by the command-line parser, causing
:: cl.exe to swallow all subsequent arguments. All paths here have no spaces.
cl.exe /std:c++17 /O2 /W3 /EHsc /MD /nologo /Fe%BUILD_DIR%\hw_test.exe /Fo%BUILD_DIR%\ ^
    /I%SCRIPT_DIR% ^
    /I%VLFD_FFI_INC% ^
    %SCRIPT_DIR%pin_mapping.cpp ^
    %SCRIPT_DIR%usb_driver.cpp ^
    %SCRIPT_DIR%test_patterns.cpp ^
    %SCRIPT_DIR%ram_test_cases.cpp ^
    %SCRIPT_DIR%hw_test.cpp ^
    /link %VLFD_FFI_DIR%\vlfd_ffi.lib ntdll.lib ws2_32.lib userenv.lib bcrypt.lib

if errorlevel 1 (
    echo [ERROR] MSVC build failed
    exit /b 1
)

goto :copy_dll

:build_gcc
echo [BUILD] Using MinGW/GCC compiler...

g++ -std=c++17 -O2 -Wall -I%SCRIPT_DIR% -I%VLFD_FFI_INC% -o %BUILD_DIR%\hw_test.exe ^
    %SCRIPT_DIR%pin_mapping.cpp ^
    %SCRIPT_DIR%usb_driver.cpp ^
    %SCRIPT_DIR%test_patterns.cpp ^
    %SCRIPT_DIR%ram_test_cases.cpp ^
    %SCRIPT_DIR%hw_test.cpp ^
    -L%VLFD_FFI_DIR% -lvlfd_ffi ^
    -lws2_32 -luserenv -lbcrypt

if errorlevel 1 (
    echo [ERROR] GCC build failed
    exit /b 1
)

goto :copy_dll

:copy_dll
copy /Y "%VLFD_FFI_DIR%\vlfd_ffi.dll" "%BUILD_DIR%\" >nul
echo [OK] Build complete: %BUILD_DIR%\hw_test.exe

copy /Y "%SCRIPT_DIR%run_hw_test.bat" "%BUILD_DIR%\" >nul 2>nul

exit /b 0
