@echo off
rem Unregisters the STL thumbnail handler (current user) and removes settings.
setlocal
regsvr32 /s /u "%~dp0StlThumbnail.dll"
reg delete "HKCU\Software\StlThumbnail" /f >nul 2>&1
echo STL thumbnail handler unregistered.
exit /b 0
