@echo off
setlocal
cd /d "%~dp0.."

set PRESET=%1
if "%PRESET%"=="" set PRESET=dev

echo ==========================================
echo [1/3] Building OpenCPP-LB (preset: %PRESET%)...
echo ==========================================
cmake --preset %PRESET% > nul || exit /b 1
cmake --build --preset %PRESET%
if %errorlevel% neq 0 (
    echo [ERROR] Build failed!
    exit /b %errorlevel%
)

echo.
echo ==========================================
echo [2/3] Running unit + integration tests...
echo ==========================================
ctest --preset %PRESET%
if %errorlevel% neq 0 (
    echo [ERROR] Tests failed!
    exit /b %errorlevel%
)

echo.
echo ==========================================
echo [3/3] End-to-end smoke test (Python backends + curl)...
echo ==========================================
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\smoke.ps1 -Preset %PRESET%
if %errorlevel% neq 0 (
    echo [ERROR] Smoke test failed!
    exit /b %errorlevel%
)
