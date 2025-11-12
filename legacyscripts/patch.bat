@echo off
cd /d %~dp0
start purchase.html
MCPatcher Minecraft.Windows.exe XStoreQueryGameLicenseAsync.binpatch.txt XStoreQueryGameLicenseResult.binpatch.txt XStoreRegisterGameLicenseChanged.binpatch.txt
pause