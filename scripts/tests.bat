@echo off
setlocal
cd /d "%~dp0.."

set PRESET=%1
if "%PRESET%"=="" set PRESET=dev

echo ==========================================
echo [1/4] Building OpenCPP-LB (preset: %PRESET%)...
echo ==========================================
cmake --preset %PRESET% > nul || exit /b 1
cmake --build --preset %PRESET%
if %errorlevel% neq 0 (
    echo [ERROR] Build failed!
    exit /b %errorlevel%
)

echo.
echo ==========================================
echo [2/4] Running tests...
echo ==========================================
ctest --preset %PRESET%
if %errorlevel% neq 0 (
    echo [ERROR] Tests failed!
    exit /b %errorlevel%
)

echo.
echo ==========================================
echo [3/4] Starting Load Balancer (Background)...
echo ==========================================
start /b build\%PRESET%\src\opencpp_lb.exe
:: Wait for the server socket to bind and start listening
ping -n 2 127.0.0.1 > nul

echo.
echo ==========================================
echo [4/4] Sending Test Request to http://localhost:8080...
echo ==========================================
curl -i --max-time 3 http://localhost:8080

taskkill /IM opencpp_lb.exe /F > nul 2>&1

echo.
echo ==========================================
echo [TEST] Test complete!
echo (Check the load balancer logs above for routing/error output).
echo ==========================================
