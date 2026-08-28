@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cd /d "%~dp0.."
cl /nologo /std:c++17 /O2 /EHsc /MT /DUNICODE /D_UNICODE test\verify_shell.cpp /Fo:build\ /Fe:build\verify_shell.exe || exit /b 1
build\verify_shell.exe test\torus.stl test\shell_thumb.bmp
exit /b %errorlevel%
