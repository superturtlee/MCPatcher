@echo off
cd /d %~dp0

echo Applying Patch1...
patcher Minecraft.Windows.exe patch.txt
if exist "patched.Minecraft.Windows.exe" (
    move Minecraft.Windows.exe Minecraft.Windows.exe.bak
    move patched.Minecraft.Windows.exe Minecraft.Windows.exe
    echo Patch1 Applyed
) else (
    echo Patch1 Failed
)

echo Applying Patch2...
patcher Minecraft.Windows.exe patch2.txt
if exist "patched.Minecraft.Windows.exe" (
    move patched.Minecraft.Windows.exe Minecraft.Windows.exe
    echo Patch2 Applyed
) else (
    echo Patch2 Failed
)
pause