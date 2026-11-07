@echo off
rem MT2009 - eterpack_py_tool (native x64, MSVC) derleme betigi. Cagri: tests\run_limited.ps1 -Build ile (%50 CPU).
setlocal
set "ROOT=%~dp0.."
pushd "%ROOT%"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
set "OUT=build\python311_migration\bin"
if not exist "%OUT%" mkdir "%OUT%"
cl.exe /nologo /std:c++20 /EHsc /MT /O2 /W3 /DWIN32 /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS /D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING /Iextern\common\include /Iextern\client\include /Iclient\src /Iclient\src\EterPack /Iclient\src\EterBase /c client\src\EterPack\EterPack.cpp /Fo:"build\obj_x64\mileslib_prod\EterPack.obj"
if errorlevel 1 exit /b 1
cl.exe /nologo /std:c++20 /EHsc /MT /O2 /W3 /DWIN32 /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS /D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING /Iextern\common\include /Iextern\client\include /Iclient\src /Iclient\src\EterPack /Iclient\src\EterBase /c tests\eterpack_py_tool.cpp /Fo:"%OUT%\eterpack_py_tool.obj"
if errorlevel 1 exit /b 1
link.exe /nologo /MACHINE:X64 /LIBPATH:extern\client\lib /OUT:"%OUT%\eterpack_py_tool.exe" "%OUT%\eterpack_py_tool.obj" ^
    build\obj_x64\mileslib_prod\EterPack.obj build\obj_x64\mileslib_prod\EterPackManager.obj build\obj_x64\mileslib_prod\EterPackPolicy_CSHybridCrypt.obj ^
    build\obj_x64\mileslib_prod\MappedFile.obj build\obj_x64\mileslib_prod\FileBase.obj build\obj_x64\mileslib_prod\FileDir.obj build\obj_x64\mileslib_prod\FileLoader.obj ^
    build\obj_x64\mileslib_prod\lzo.obj build\obj_x64\mileslib_prod\tea.obj build\obj_x64\mileslib_prod\CRC32.obj build\obj_x64\mileslib_prod\Timer.obj ^
    build\obj_x64\mileslib_prod\Utils.obj build\obj_x64\mileslib_prod\Random.obj build\obj_x64\mileslib_prod\Debug.obj build\obj_x64\mileslib_prod\Stl.obj ^
    extern\client\lib\cryptlib-x64.lib extern\client\lib\lzo2_x64.lib ole32.lib user32.lib winmm.lib advapi32.lib
set "RC=%ERRORLEVEL%"
popd
exit /b %RC%
