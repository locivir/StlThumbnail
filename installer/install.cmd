@echo off
rem Installs the STL thumbnail handler for the current user (no admin needed).
rem Everything needed is in this folder - no runtimes or redistributables required.
setlocal
regsvr32 /s "%~dp0StlThumbnail.dll" || (echo registration failed & exit /b 1)
echo STL thumbnail handler registered for the current user.
echo Configure previews (angle, color, background, lighting) with StlThumbConfig.exe.
echo (Explorer may show old cached thumbnails; press F5 in the folder to refresh.)
exit /b 0
