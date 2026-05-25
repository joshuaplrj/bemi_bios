@echo off
setlocal enabledelayedexpansion
title Bemi App Launcher
cd /d "%~dp0"

echo.
echo  ========================================
echo    Bemi App - LLM Inference Accelerator
echo    Windows One-Click Launcher
echo  ========================================
echo.
echo  Checking Python...

python --version >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo  [ERROR] Python not found. Please install Python 3.9+ from https://python.org
    echo  Make sure to check "Add Python to PATH" during installation.
    pause
    exit /b 1
)

echo  Python found. Checking dependencies...
echo.

python -c "import tkinter" 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo  [WARNING] tkinter not found. It should be included with Python.
    echo  If you installed Python from the Microsoft Store, tkinter may be missing.
    echo  Please reinstall Python from https://python.org
    pause
    exit /b 1
)

echo  Launching Bemi App...
echo.

python "%~dp0bemi_app\app.py"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo  [ERROR] Application exited with code %ERRORLEVEL%
    pause
)
