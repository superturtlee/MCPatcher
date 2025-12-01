@echo off
setlocal enabledelayedexpansion

REM Folia Feature Patches Merger Script (Windows Wrapper)
REM 
REM This batch file wraps the merge-patches.sh script for Windows users.
REM It requires Git Bash or WSL to be installed.

REM Check if Git Bash is available
where bash >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: bash not found. Please install Git Bash or WSL.
    exit /b 1
)

REM Get the directory where this script is located
set "SCRIPT_DIR=%~dp0"

REM Run the bash script with all arguments
REM Git Bash handles Windows paths automatically
bash "%SCRIPT_DIR%merge-patches.sh" %*
