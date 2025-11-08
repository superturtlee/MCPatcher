@echo off
cd /d %~dp0
del /f /s /q %appdata%\..\Local\Temp\mcbe
start minecraft-preview://
injector dump
move %appdata%\..\Local\Temp\mcbe .\
MCPatcher mcbe\Minecraft.Windows.exe XStoreQueryGameLicenseAsync.binpatch.txt XStoreQueryGameLicenseAsyncResult.binpatch.txt
pause