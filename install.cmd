@echo off
rem Installs the STL thumbnail handler for the current user (no admin needed).
rem 1) builds are expected in build\ (run build.cmd first)
rem 2) registers the COM thumbnail provider
setlocal
cd /d "%~dp0"
if not exist build\StlThumbnail.dll (
    echo build\StlThumbnail.dll not found - run build.cmd first.
    exit /b 1
)
regsvr32 /s "%~dp0build\StlThumbnail.dll" || (echo registration failed & exit /b 1)
echo STL thumbnail handler registered for the current user.
echo Configure it with the helper app: config-app\bin\Release\net10.0-windows\StlThumbConfig.exe
echo (Explorer may show old cached thumbnails; press F5 in the folder or clear the thumbnail cache.)
exit /b 0
