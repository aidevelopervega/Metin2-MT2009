@echo off
rem MT2009 - test_python311_pack_runtime_x64 (native x64, MSVC) derleme betigi. Cagri: tests\run_limited.ps1 -Build ile (%50 CPU).
rem On kosul: build\obj_x64\py3_real\*.obj ve build\obj_x64\mileslib_prod\*.obj (uretim istemci kaynaklarindan x64 derlenmis nesneler).
setlocal
set "ROOT=%~dp0.."
pushd "%ROOT%"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
set "PY311_DIR=C:\Users\oyun\AppData\Local\Programs\Python\Python311"
set "OUT=build\python311_migration\runtime"
if not exist "%OUT%" mkdir "%OUT%"
cl.exe /nologo /std:c++20 /EHsc /MT /O2 /W3 /DWIN32 /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS /D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING /DMT2_USE_PYTHON3=1 /DMT2_PYTHON_MODULE_ONLY=1 /I"%PY311_DIR%\include" /Iextern\common\include /Iextern\client\include /Iclient\src /Iclient\src\EterPack /Iclient\src\EterBase /c tests\test_python311_pack_runtime_x64.cpp /Fo:"%OUT%\test_python311_pack_runtime_x64.obj"
if errorlevel 1 exit /b 1
link.exe /nologo /MACHINE:X64 /LIBPATH:extern\client\lib /OUT:"%OUT%\test_python311_pack_runtime_x64.exe" "%OUT%\test_python311_pack_runtime_x64.obj" ^
    build\obj_x64\py3_real\PythonUtils.obj build\obj_x64\py3_real\PythonLauncher.obj build\obj_x64\py3_real\PythonMarshal.obj ^
    build\obj_x64\py3_real\PythonDebugModule.obj build\obj_x64\py3_real\PythonliblibManager.obj build\obj_x64\py3_real\PythonPackModule.obj ^
    build\obj_x64\mileslib_prod\EterPack.obj build\obj_x64\mileslib_prod\EterPackManager.obj build\obj_x64\mileslib_prod\EterPackPolicy_CSHybridCrypt.obj ^
    build\obj_x64\mileslib_prod\MappedFile.obj build\obj_x64\mileslib_prod\FileBase.obj build\obj_x64\mileslib_prod\FileDir.obj build\obj_x64\mileslib_prod\FileLoader.obj ^
    build\obj_x64\mileslib_prod\lzo.obj build\obj_x64\mileslib_prod\tea.obj build\obj_x64\mileslib_prod\CRC32.obj build\obj_x64\mileslib_prod\Timer.obj ^
    build\obj_x64\mileslib_prod\Utils.obj build\obj_x64\mileslib_prod\Random.obj build\obj_x64\mileslib_prod\Debug.obj build\obj_x64\mileslib_prod\Stl.obj ^
    extern\client\lib\cryptlib-x64.lib extern\client\lib\lzo2_x64.lib "%PY311_DIR%\libs\python311.lib" ole32.lib user32.lib winmm.lib advapi32.lib
set "RC=%ERRORLEVEL%"
if "%RC%"=="0" (
  copy /y "%PY311_DIR%\python311.dll" "%OUT%\python311.dll" >nul
  if exist "%OUT%\lib" rmdir /s /q "%OUT%\lib"
  xcopy /e /i /q /y client\files\lib "%OUT%\lib" >nul
)
popd
exit /b %RC%
