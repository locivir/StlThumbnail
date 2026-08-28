@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cd /d "%~dp0.."

echo === OBJ parser regression (self-contained, no shell surrogate) ===
cl /nologo /std:c++17 /O2 /EHsc /MT test\test_obj.cpp src\renderer.cpp /Fo:build\ /Fe:build\test_obj.exe || exit /b 1
build\test_obj.exe test\test_stl.obj 1024 || (echo REGRESSION FAILED: test_stl.obj & exit /b 1)
build\test_obj.exe test\test_nurbs.obj 0 || (echo REGRESSION FAILED: test_nurbs.obj & exit /b 1)
build\test_obj.exe test\cube.obj 12 || (echo REGRESSION FAILED: cube.obj & exit /b 1)

echo.
echo === Shell-pipeline check (IShellItemImageFactory, Explorer's path) ===
cl /nologo /std:c++17 /O2 /EHsc /MT /DUNICODE /D_UNICODE test\verify_shell.cpp /Fo:build\ /Fe:build\verify_shell.exe || exit /b 1
build\verify_shell.exe "%~dp0torus.stl" "%~dp0shell_thumb.bmp"
exit /b %errorlevel%
