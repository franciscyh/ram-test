@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

::==============================================================================
:: UFDE+ Yosys Flow Runner
:: Usage: run_flow.bat <config_dir>
:: Example: run_flow.bat test\4x1024
::==============================================================================

::==============================================================================
:: Setup Paths
::==============================================================================
set "SCRIPT_DIR=%~dp0"

:: Tools (使用本地 bin 目录)
set "TOOLS_DIR=%SCRIPT_DIR%bin"
set "RESOURCE_DIR=%TOOLS_DIR%\resource"

:: Add tools to PATH
set "PATH=%TOOLS_DIR%;%PATH%"

set "CONFIG_DIR=%~1"
if "%CONFIG_DIR%"=="" (
    echo Usage: run_flow.bat ^<config_dir^>
    echo Example: run_flow.bat test\4x1024
    exit /b 1
)

:: Convert relative path to absolute
set "CONFIG_PATH=%SCRIPT_DIR%%CONFIG_DIR%"
if not exist "%CONFIG_PATH%" (
    echo Error: Directory not found: %CONFIG_PATH%
    exit /b 1
)

:: Change to IP-Generator directory
pushd "%SCRIPT_DIR%"
if errorlevel 1 (
    echo Error: Cannot change to script directory: %SCRIPT_DIR%
    exit /b 1
)

:: Set Yosys executable (static path)
set "YOSYS_EXE=%TOOLS_DIR%\yosys.exe"
if not exist "%YOSYS_EXE%" (
    echo Error: Yosys not found: %YOSYS_EXE%
    popd
    exit /b 1
)

:: Resource files
set "YOSYS_TCL=%RESOURCE_DIR%\yosys\yosys_fde.tcl"
set "SIMLIB=%RESOURCE_DIR%\yosys\fdesimlib.v"
set "TECHMAP=%RESOURCE_DIR%\yosys\techmap.v"
set "CELLS_MAP=%RESOURCE_DIR%\yosys\cells_map.v"
set "DC_CELL=%RESOURCE_DIR%\hw_lib\dc_cell.xml"
set "FDP3_CELL=%RESOURCE_DIR%\hw_lib\fdp3_cell.xml"
set "FDP3_DCP=%RESOURCE_DIR%\hw_lib\fdp3_dcplib.xml"
set "FDP3_CFG=%RESOURCE_DIR%\hw_lib\fdp3_config.xml"
set "ARCH=%RESOURCE_DIR%\hw_lib\fdp3p7_arch.xml"
set "DELAY=%RESOURCE_DIR%\hw_lib\fdp3p7_dly.xml"
set "CIL=%RESOURCE_DIR%\hw_lib\fdp3p7_cil.xml"

::==============================================================================
:: Check Input Files
::==============================================================================
echo ========================================
echo UFDE+ Yosys Flow
echo Config: %CONFIG_PATH%
echo ========================================
echo.

if not exist "%CONFIG_PATH%\top.v" (
    echo Error: top.v not found in %CONFIG_PATH%
    popd
    exit /b 1
)
if not exist "%CONFIG_PATH%\test.v" (
    echo Error: test.v not found in %CONFIG_PATH%
    popd
    exit /b 1
)
if not exist "%CONFIG_PATH%\top_cons.xml" (
    echo Error: top_cons.xml not found in %CONFIG_PATH%
    popd
    exit /b 1
)

::==============================================================================
:: Step 1: Synthesis
::==============================================================================
cd /d "%CONFIG_PATH%"

:: Clean previous results (must be done inside config dir)
echo Cleaning previous results...
del /f /q "top_yosys_syn.edf" "top_yosys_syn.log" 2>nul
del /f /q "top_yosys_map.xml" "top_yosys_map.v" "top_yosys_map.log" 2>nul
del /f /q "top_yosys_pack.xml" "top_yosys_pack.v" "top_yosys_pack.log" 2>nul
del /f /q "top_yosys_place.xml" "top_yosys_place.log" 2>nul
del /f /q "top_yosys_route.xml" "top_yosys_route.v" "top_yosys_route.log" 2>nul
del /f /q "top_yosys_bit.bit" "top_yosys_bit.log" 2>nul
echo   OK: previous results cleaned
echo.

echo [Step 1/6] Synthesis...

set "SYN_OUT=top_yosys_syn.edf"
set "SYN_LOG=top_yosys_syn.log"

"%YOSYS_EXE%" -p "tcl %YOSYS_TCL% -l %SIMLIB% -m %TECHMAP% -c %CELLS_MAP% -o %SYN_OUT%" top.v test.v > %SYN_LOG% 2>&1
if errorlevel 1 (
    echo Error: Synthesis failed
    echo See log: %CONFIG_PATH%\%SYN_LOG%
    type %SYN_LOG% 2>nul | findstr /i "error"
    popd
    exit /b 1
)
echo   OK: %SYN_OUT%

::==============================================================================
:: Step 2: Mapping
::==============================================================================
echo.
echo [Step 2/6] Mapping...

:: Set map tool (static path)
set "MAP_TOOL=%TOOLS_DIR%\map.exe"

set "MAP_OUT=top_yosys_map.xml"
set "MAP_V=top_yosys_map.v"
set "MAP_LOG=top_yosys_map.log"

"%MAP_TOOL%" -y -i %SYN_OUT% -o %MAP_OUT% -c "%DC_CELL%" -v %MAP_V% > %MAP_LOG% 2>&1
if errorlevel 1 (
    echo Error: Mapping failed
    echo See log: %CONFIG_PATH%\%MAP_LOG%
    type %MAP_LOG% 2>nul | findstr /i "error"
    popd
    exit /b 1
)
echo   OK: %MAP_OUT%

::==============================================================================
:: Step 3: Packing
::==============================================================================
echo.
echo [Step 3/6] Packing...

:: Set pack tool (static path)
set "PACK_TOOL=%TOOLS_DIR%\pack.exe"

set "PACK_OUT=top_yosys_pack.xml"
set "PACK_V=top_yosys_pack.v"
set "PACK_LOG=top_yosys_pack.log"

"%PACK_TOOL%" -c fdp3 -n %MAP_OUT% -l "%FDP3_CELL%" -r "%FDP3_DCP%" -o %PACK_OUT% -g "%FDP3_CFG%" -s %PACK_V% > %PACK_LOG% 2>&1
if errorlevel 1 (
    echo Error: Packing failed
    echo See log: %CONFIG_PATH%\%PACK_LOG%
    type %PACK_LOG% 2>nul | findstr /i "error"
    popd
    exit /b 1
)
echo   OK: %PACK_OUT%

::==============================================================================
:: Step 4: Placing
::==============================================================================
echo.
echo [Step 4/6] Placing...

:: Set place tool (static path)
set "PLACE_TOOL=%TOOLS_DIR%\place.exe"

set "PLACE_OUT=top_yosys_place.xml"
set "PLACE_LOG=top_yosys_place.log"

"%PLACE_TOOL%" -a "%ARCH%" -d "%DELAY%" -i %PACK_OUT% -o %PLACE_OUT% -c top_cons.xml -b > %PLACE_LOG% 2>&1
if errorlevel 1 (
    echo Error: Placing failed
    echo See log: %CONFIG_PATH%\%PLACE_LOG%
    type %PLACE_LOG% 2>nul | findstr /i "error"
    popd
    exit /b 1
)
echo   OK: %PLACE_OUT%

::==============================================================================
:: Step 5: Routing
::==============================================================================
echo.
echo [Step 5/6] Routing...

:: Set route tool (static path)
set "ROUTE_TOOL=%TOOLS_DIR%\route.exe"

set "ROUTE_OUT=top_yosys_route.xml"
set "ROUTE_V=top_yosys_route.v"
set "ROUTE_LOG=top_yosys_route.log"

"%ROUTE_TOOL%" -a "%ARCH%" -n %PLACE_OUT% -o %ROUTE_OUT% -d -c top_cons.xml -i "%CIL%" -v %ROUTE_V% > %ROUTE_LOG% 2>&1
if errorlevel 1 (
    echo Error: Routing failed
    echo See log: %CONFIG_PATH%\%ROUTE_LOG%
    type %ROUTE_LOG% 2>nul | findstr /i "error"
    popd
    exit /b 1
)
echo   OK: %ROUTE_OUT%

::==============================================================================
:: Step 6: BitGen
::==============================================================================
echo.
echo [Step 6/6] Generating bitstream...

:: Set bitgen tool (static path)
set "BITGEN_TOOL=%TOOLS_DIR%\bitgen.exe"

set "BIT_OUT=top_yosys_bit.bit"
set "BIT_LOG=top_yosys_bit.log"

"%BITGEN_TOOL%" -a "%ARCH%" -c "%CIL%" -n %ROUTE_OUT% -b %BIT_OUT% > %BIT_LOG% 2>&1
if errorlevel 1 (
    echo Error: BitGen failed
    echo See log: %CONFIG_PATH%\%BIT_LOG%
    type %BIT_LOG% 2>nul | findstr /i "error"
    popd
    exit /b 1
)
echo   OK: %BIT_OUT%

::==============================================================================
:: Summary
::==============================================================================
popd
echo.
echo ========================================
echo Flow completed successfully!
echo Output: %CONFIG_PATH%\%BIT_OUT%
echo ========================================

exit /b 0
