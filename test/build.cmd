@echo off
setlocal
set errorcount=0

call :build Win32 Debug "%~dp0..\ISPC Texture Compressor\ispc_texcomp\ispc_texcomp.vcxproj"
call :build x64   Debug "%~dp0..\ISPC Texture Compressor\ispc_texcomp\ispc_texcomp.vcxproj"
call :build Win32 Release "%~dp0..\ISPC Texture Compressor\ispc_texcomp\ispc_texcomp.vcxproj"
call :build x64   Release "%~dp0..\ISPC Texture Compressor\ispc_texcomp\ispc_texcomp.vcxproj"

call :build Win32 Debug "%~dp0..\ISPC Texture Compressor\test_astc.sln"
call :build x64   Debug "%~dp0..\ISPC Texture Compressor\test_astc.sln"
call :build Win32 Release "%~dp0..\ISPC Texture Compressor\test_astc.sln"
call :build x64   Release "%~dp0..\ISPC Texture Compressor\test_astc.sln"

call :build Win32 Debug "%~dp0..\ISPC Texture Compressor\ISPC Texture Compressor.sln"
call :build x64   Debug "%~dp0..\ISPC Texture Compressor\ISPC Texture Compressor.sln"
call :build Win32 Release "%~dp0..\ISPC Texture Compressor\ISPC Texture Compressor.sln"
call :build x64   Release "%~dp0..\ISPC Texture Compressor\ISPC Texture Compressor.sln"

call :build Win32 Debug "%~dp0..\ISPC Texture Compressor\ISPC HDR Texture Compressor.sln"
call :build x64   Debug "%~dp0..\ISPC Texture Compressor\ISPC HDR Texture Compressor.sln"
call :build Win32 Release "%~dp0..\ISPC Texture Compressor\ISPC HDR Texture Compressor.sln"
call :build x64   Release "%~dp0..\ISPC Texture Compressor\ISPC HDR Texture Compressor.sln"

echo.
if "%errorcount%"=="0" (echo PASS) else (echo %errorcount% FAILED)
exit /b %errorcount%

:build
    msbuild /nologo /verbosity:minimal /p:Platform=%1 /p:Configuration=%2 %3
    if not "%errorlevel%"=="0" set /a errorcount=%errorcount%+1
    exit /b 0
