@echo off
cd /d %~dp0
start purchase.html
MCPatcher Minecraft.Windows.exe XStoreQueryGameLicenseAsync.binpatch.txt XStoreQueryGameLicenseAsyncResult.binpatch.txt
pause