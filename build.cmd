@echo off
rem Build StlThumbnail.dll with MSVC (x64). Run from a normal shell.
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist build mkdir build
cl /nologo /std:c++17 /O2 /EHsc /W3 /LD /MT /DUNICODE /D_UNICODE ^
   src\renderer.cpp src\provider.cpp src\capi.cpp ^
   /Fo:build\ /Fe:build\StlThumbnail.dll ^
   /link /DEF:src\StlThumbnail.def shlwapi.lib gdi32.lib advapi32.lib shell32.lib ole32.lib user32.lib
exit /b %errorlevel%
