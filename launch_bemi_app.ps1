# Bemi App - PowerShell Launcher
$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $scriptDir

Write-Host ""
Write-Host "  ========================================" -ForegroundColor Cyan
Write-Host "    Bemi App - LLM Inference Accelerator" -ForegroundColor Cyan
Write-Host "    Windows One-Click Launcher" -ForegroundColor Cyan
Write-Host "  ========================================" -ForegroundColor Cyan
Write-Host ""

# Check Python
try {
    $pythonVersion = python --version 2>&1
    Write-Host "  Python: $pythonVersion" -ForegroundColor Green
} catch {
    Write-Host "  [ERROR] Python not found. Install Python 3.9+ from https://python.org" -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}

# Check tkinter
python -c "import tkinter" 2>$null
if ($LASTEXITCODE -ne 0) {
    Write-Host "  [WARNING] tkinter not available. Please ensure Python includes tkinter." -ForegroundColor Yellow
}

Write-Host "  Launching Bemi App..." -ForegroundColor Cyan
Write-Host ""

python "$scriptDir\bemi_app\app.py"

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "  [ERROR] Application exited with code $LASTEXITCODE" -ForegroundColor Red
    Read-Host "Press Enter to exit"
}
