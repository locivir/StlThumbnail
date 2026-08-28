@echo off
rem Builds the complete zero-dependency distribution folder: dist\
rem   StlThumbConfig.exe  (self-contained single-file, no .NET install needed)
rem   StlThumbnail.dll    (native handler, statically linked CRT)
rem   sample.stl, install.cmd, uninstall.cmd
setlocal
cd /d "%~dp0"
call build.cmd || exit /b 1
dotnet publish config-app -c Release -r win-x64 --self-contained -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -o dist || exit /b 1
del /q dist\StlThumbConfig.pdb 2>nul
copy /y build\StlThumbnail.dll dist\ >nul
if exist test\torus.stl copy /y test\torus.stl dist\sample.stl >nul
copy /y installer\install.cmd dist\ >nul
copy /y installer\uninstall.cmd dist\ >nul
echo.
echo dist\ is ready - copy the folder to any Windows 11 x64 machine and run install.cmd.
exit /b 0
