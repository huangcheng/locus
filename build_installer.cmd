@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
echo VCVARS ERR=%ERRORLEVEL%
set "PATH=C:\Qt\Tools\Ninja;C:\Qt\Tools\CMake_64\bin;%PATH%"
cmake --build cmake-build-release --parallel
echo BUILD ERR=%ERRORLEVEL%
if errorlevel 1 exit /b 1
cmake --install cmake-build-release --prefix cmake-build-release\stage
echo INSTALL ERR=%ERRORLEVEL%
if errorlevel 1 exit /b 1
"C:\Users\huang\AppData\Local\Programs\Inno Setup 6\ISCC.exe" installer.iss
echo ISCC ERR=%ERRORLEVEL%
if errorlevel 1 exit /b 1
echo INSTALLER_BUILD_OK
