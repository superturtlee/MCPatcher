@echo off
REM Batch wrapper for build-patch.ps1
REM This script builds PATCHED sections from .asm files using MASM

setlocal

set "SCRIPT_DIR=%~dp0"
set "PS_SCRIPT=%SCRIPT_DIR%build-patch.ps1"

REM Check if PowerShell script exists
if not exist "%PS_SCRIPT%" (
    echo ERROR: build-patch.ps1 not found!
    exit /b 1
)

REM Run PowerShell script with all parameters
copy /Y patterns\* .
powershell.exe -ExecutionPolicy Bypass -File "%PS_SCRIPT%" %*
powershell.exe -ExecutionPolicy Bypass -File "%PS_SCRIPT%" -GeneratePattern
exit /b %ERRORLEVEL%
