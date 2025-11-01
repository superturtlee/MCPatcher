@echo off
cd /d %~dp0
MCPatcher Minecraft.Windows.exe XStoreQueryGameLicenseAsync.binpatch.txt XStoreQueryGameLicenseAsyncResult.binpatch.txt
pause