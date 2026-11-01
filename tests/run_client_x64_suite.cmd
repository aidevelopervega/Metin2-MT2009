@echo off
setlocal
if exist "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" >nul
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
) else (
    echo [ERROR] vcvars64.bat not found
    exit /b 1
)
if errorlevel 1 exit /b 1

set "PY311_DIR=C:\Users\oyun\AppData\Local\Programs\Python\Python311"
set "PATH=%CD%\extern\client\dll;%PY311_DIR%;%PATH%"

echo [0/6] Verifying SDK Inventory Lockfile, Header Parity, Binary Architectures and Zero Drift...
python tests\verify_sdk_inventory.py
if errorlevel 1 exit /b 1

if not exist "build\obj_x64\mileslib_prod" mkdir "build\obj_x64\mileslib_prod"

if exist "build\obj_x64\test_miniaudio_x64_prototype.exe" goto :have_proto
echo [1/6] Compiling test_miniaudio_x64_prototype.exe [PE32+ AMD64]...
cl.exe /nologo /std:c++20 /EHsc /MT /O2 /W3 /Iextern\client\include tests\test_miniaudio_x64_prototype.cpp /Fe:build\obj_x64\test_miniaudio_x64_prototype.exe /Fo:build\obj_x64\test_miniaudio_x64_prototype.obj /link /MACHINE:X64 ole32.lib user32.lib winmm.lib
if errorlevel 1 exit /b 1
:have_proto

build\obj_x64\test_miniaudio_x64_prototype.exe
if errorlevel 1 exit /b 1

echo [2/6] Checking MilesLib/EterPack/EterBase AMD64 COFF objects (with real Crypto++ 8.9.0 and LZO 2.10)...
if exist "build\obj_x64\mileslib_prod\cipher.obj" goto :have_miles_objs
echo [2b/6] Compiling MilesLib + EterPack + EterBase AMD64 COFF objects...
cl.exe /nologo /std:c++20 /EHsc /MT /O2 /W3 /DWIN32 /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS /D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING /DMT2_USE_MINIAUDIO=1 /Iextern\common\include /Iextern\client\include /Iclient\src /Iclient\src\MilesLib /Iclient\src\EterPack /Iclient\src\EterBase /c ^
    client\src\MilesLib\SoundBase.cpp ^
    client\src\MilesLib\SoundData.cpp ^
    client\src\MilesLib\SoundInstance2D.cpp ^
    client\src\MilesLib\SoundInstance3D.cpp ^
    client\src\MilesLib\SoundInstanceStream.cpp ^
    client\src\MilesLib\SoundManager.cpp ^
    client\src\MilesLib\SoundManager2D.cpp ^
    client\src\MilesLib\SoundManager3D.cpp ^
    client\src\MilesLib\SoundManagerStream.cpp ^
    client\src\MilesLib\Type.cpp ^
    client\src\EterPack\EterPack.cpp ^
    client\src\EterPack\EterPackManager.cpp ^
    client\src\EterPack\EterPackPolicy_CSHybridCrypt.cpp ^
    client\src\EterBase\MappedFile.cpp ^
    client\src\EterBase\FileBase.cpp ^
    client\src\EterBase\FileDir.cpp ^
    client\src\EterBase\FileLoader.cpp ^
    client\src\EterBase\lzo.cpp ^
    client\src\EterBase\tea.cpp ^
    client\src\EterBase\CRC32.cpp ^
    client\src\EterBase\cipher.cpp ^
    client\src\EterBase\Timer.cpp ^
    client\src\EterBase\Utils.cpp ^
    client\src\EterBase\Random.cpp ^
    client\src\EterBase\Debug.cpp ^
    client\src\EterBase\Stl.cpp ^
    /Fo:build\obj_x64\mileslib_prod\
if errorlevel 1 exit /b 1
:have_miles_objs

if exist "build\obj_x64\mileslib_prod\MiniaudioEngine.obj" goto :have_ma_engine
cl.exe /nologo /std:c++20 /EHsc /MT /O2 /W3 /DWIN32 /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS /D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING /DMT2_USE_MINIAUDIO=1 /Iextern\common\include /Iextern\client\include /Iclient\src /Iclient\src\MilesLib /Iclient\src\EterPack /Iclient\src\EterBase /c ^
    client\src\MilesLib\MiniaudioEngine.cpp ^
    /Fo:build\obj_x64\mileslib_prod\
if errorlevel 1 exit /b 1
:have_ma_engine

if exist "build\obj_x64\test_mileslib_miniaudio_production_x64.exe" goto :have_miles_exe
echo [3/6] Compiling test_mileslib_miniaudio_production_x64.cpp and linking PE32+ executable with cryptlib-x64.lib + lzo2_x64.lib...
cl.exe /nologo /std:c++20 /EHsc /MT /O2 /W3 /DWIN32 /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS /D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING /DMT2_USE_MINIAUDIO=1 /Iextern\common\include /Iextern\client\include /Iclient\src /Iclient\src\MilesLib /Iclient\src\EterPack /Iclient\src\EterBase /c ^
    tests\test_mileslib_miniaudio_production_x64.cpp ^
    /Fo:build\obj_x64\mileslib_prod\
if errorlevel 1 exit /b 1

link.exe /nologo /MACHINE:X64 /LIBPATH:extern\client\lib /OUT:build\obj_x64\test_mileslib_miniaudio_production_x64.exe ^
    build\obj_x64\mileslib_prod\test_mileslib_miniaudio_production_x64.obj ^
    build\obj_x64\mileslib_prod\MiniaudioEngine.obj ^
    build\obj_x64\mileslib_prod\SoundBase.obj ^
    build\obj_x64\mileslib_prod\SoundData.obj ^
    build\obj_x64\mileslib_prod\SoundInstance2D.obj ^
    build\obj_x64\mileslib_prod\SoundInstance3D.obj ^
    build\obj_x64\mileslib_prod\SoundInstanceStream.obj ^
    build\obj_x64\mileslib_prod\SoundManager.obj ^
    build\obj_x64\mileslib_prod\SoundManager2D.obj ^
    build\obj_x64\mileslib_prod\SoundManager3D.obj ^
    build\obj_x64\mileslib_prod\SoundManagerStream.obj ^
    build\obj_x64\mileslib_prod\EterPack.obj ^
    build\obj_x64\mileslib_prod\EterPackManager.obj ^
    build\obj_x64\mileslib_prod\EterPackPolicy_CSHybridCrypt.obj ^
    build\obj_x64\mileslib_prod\MappedFile.obj ^
    build\obj_x64\mileslib_prod\FileBase.obj ^
    build\obj_x64\mileslib_prod\FileDir.obj ^
    build\obj_x64\mileslib_prod\FileLoader.obj ^
    build\obj_x64\mileslib_prod\lzo.obj ^
    build\obj_x64\mileslib_prod\tea.obj ^
    build\obj_x64\mileslib_prod\CRC32.obj ^
    build\obj_x64\mileslib_prod\Timer.obj ^
    build\obj_x64\mileslib_prod\Utils.obj ^
    build\obj_x64\mileslib_prod\Random.obj ^
    build\obj_x64\mileslib_prod\Debug.obj ^
    build\obj_x64\mileslib_prod\Stl.obj ^
    extern\client\lib\cryptlib-x64.lib ^
    extern\client\lib\lzo2_x64.lib ^
    ole32.lib user32.lib winmm.lib advapi32.lib
if errorlevel 1 exit /b 1
:have_miles_exe

dumpbin.exe /headers build\obj_x64\test_mileslib_miniaudio_production_x64.exe | findstr /C:"8664 machine (x64)" /C:"PE32+"
if errorlevel 1 exit /b 1

build\obj_x64\test_mileslib_miniaudio_production_x64.exe
if errorlevel 1 exit /b 1

echo [4/6] Compiling and linking real Python 3.11 x64 client modules suite [PE32+ AMD64]...
if not exist "build\obj_x64\py3_real" mkdir "build\obj_x64\py3_real"

if exist "build\obj_x64\py3_real\PythonPackModule.obj" goto :have_py3_base_objs
cl.exe /nologo /std:c++20 /EHsc /MT /O2 /W3 /DWIN32 /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS /D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING /DMT2_USE_PYTHON3=1 /DMT2_PYTHON_MODULE_ONLY=1 /I"%PY311_DIR%\include" /Iextern\common\include /Iextern\client\include /Iclient\src /Iclient\src\EterPack /Iclient\src\EterBase /c ^
    client\src\ScriptLib\PythonUtils.cpp ^
    client\src\ScriptLib\PythonMarshal.cpp ^
    client\src\ScriptLib\PythonDebugModule.cpp ^
    client\src\UserInterface\PythonliblibManager.cpp ^
    client\src\UserInterface\PythonPackModule.cpp ^
    /Fo:build\obj_x64\py3_real\
if errorlevel 1 exit /b 1
:have_py3_base_objs

if exist "build\obj_x64\test_python3_real_modules_x64.exe" goto :have_py3_real_exe
cl.exe /nologo /std:c++20 /EHsc /MT /O2 /W3 /DWIN32 /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS /D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING /DMT2_USE_PYTHON3=1 /DMT2_PYTHON_MODULE_ONLY=1 /I"%PY311_DIR%\include" /Iextern\common\include /Iextern\client\include /Iclient\src /Iclient\src\EterPack /Iclient\src\EterBase /c ^
    client\src\ScriptLib\PythonLauncher.cpp ^
    tests\test_python3_real_modules_x64.cpp ^
    /Fo:build\obj_x64\py3_real\
if errorlevel 1 exit /b 1

link.exe /nologo /MACHINE:X64 /LIBPATH:extern\client\lib /NODEFAULTLIB:python27.lib /NODEFAULTLIB:liblib.lib /OUT:build\obj_x64\test_python3_real_modules_x64.exe ^
    build\obj_x64\py3_real\test_python3_real_modules_x64.obj ^
    build\obj_x64\py3_real\PythonUtils.obj ^
    build\obj_x64\py3_real\PythonLauncher.obj ^
    build\obj_x64\py3_real\PythonMarshal.obj ^
    build\obj_x64\py3_real\PythonDebugModule.obj ^
    build\obj_x64\py3_real\PythonliblibManager.obj ^
    build\obj_x64\py3_real\PythonPackModule.obj ^
    build\obj_x64\mileslib_prod\EterPack.obj ^
    build\obj_x64\mileslib_prod\EterPackManager.obj ^
    build\obj_x64\mileslib_prod\EterPackPolicy_CSHybridCrypt.obj ^
    build\obj_x64\mileslib_prod\MappedFile.obj ^
    build\obj_x64\mileslib_prod\FileBase.obj ^
    build\obj_x64\mileslib_prod\FileDir.obj ^
    build\obj_x64\mileslib_prod\FileLoader.obj ^
    build\obj_x64\mileslib_prod\lzo.obj ^
    build\obj_x64\mileslib_prod\tea.obj ^
    build\obj_x64\mileslib_prod\CRC32.obj ^
    build\obj_x64\mileslib_prod\Timer.obj ^
    build\obj_x64\mileslib_prod\Utils.obj ^
    build\obj_x64\mileslib_prod\Random.obj ^
    build\obj_x64\mileslib_prod\Debug.obj ^
    build\obj_x64\mileslib_prod\Stl.obj ^
    extern\client\lib\cryptlib-x64.lib ^
    extern\client\lib\lzo2_x64.lib ^
    "%PY311_DIR%\libs\python311.lib" ^
    ole32.lib user32.lib winmm.lib advapi32.lib
if errorlevel 1 exit /b 1
:have_py3_real_exe

dumpbin.exe /headers build\obj_x64\test_python3_real_modules_x64.exe | findstr /C:"8664 machine (x64)" /C:"PE32+"
if errorlevel 1 exit /b 1

build\obj_x64\test_python3_real_modules_x64.exe
if errorlevel 1 exit /b 1

if exist "build\obj_x64\test_python3_x64_bridge.exe" (
    build\obj_x64\test_python3_x64_bridge.exe
    if errorlevel 1 exit /b 1
)

echo [5/6] Compiling EterLib/GameLib/SpeedTreeLib/UserInterface permanent D3D9Ex AMD64 COFF objects and running test_d3d9ex_x64.exe [PE32+ AMD64]...
if not exist "build\obj_x64\eterlib_d3d9ex" mkdir "build\obj_x64\eterlib_d3d9ex"
if exist "build\obj_x64\eterlib_d3d9ex\PythonApplication.obj" goto :have_eterlib_d3d9ex_objs
cl.exe /nologo /std:c++20 /EHsc /MT /O2 /W3 /DWIN32 /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS /D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING /DMT2_USE_PYTHON3=1 /I"%PY311_DIR%\include" /Iextern\common\include /Iextern\client\include /Iclient\src /Iclient\src\EterLib /Iclient\src\EterBase /Iclient\src\EterPack /Iclient\src\EterImageLib /Iclient\src\GameLib /Iclient\src\SpeedTreeLib /c ^
    client\src\EterLib\GrpBase.cpp ^
    client\src\EterLib\GrpDevice.cpp ^
    client\src\EterLib\GrpScreen.cpp ^
    client\src\EterLib\GrpIndexBuffer.cpp ^
    client\src\EterLib\GrpVertexBuffer.cpp ^
    client\src\EterLib\GrpVertexBufferStatic.cpp ^
    client\src\EterLib\BlockTexture.cpp ^
    client\src\EterLib\GrpImageTexture.cpp ^
    client\src\EterLib\GrpTexture.cpp ^
    client\src\GameLib\AreaTerrain.cpp ^
    client\src\GameLib\DungeonBlock.cpp ^
    client\src\GameLib\SnowEnvironment.cpp ^
    client\src\GameLib\TerrainPatch.cpp ^
    client\src\SpeedTreeLib\SpeedTreeWrapper.cpp ^
    client\src\UserInterface\PythonMiniMap.cpp ^
    client\src\UserInterface\PythonApplication.cpp ^
    /Fo:build\obj_x64\eterlib_d3d9ex\
if errorlevel 1 exit /b 1
:have_eterlib_d3d9ex_objs

dumpbin.exe /headers build\obj_x64\eterlib_d3d9ex\GrpDevice.obj build\obj_x64\eterlib_d3d9ex\GrpScreen.obj build\obj_x64\eterlib_d3d9ex\GrpImageTexture.obj build\obj_x64\eterlib_d3d9ex\AreaTerrain.obj build\obj_x64\eterlib_d3d9ex\SpeedTreeWrapper.obj build\obj_x64\eterlib_d3d9ex\PythonMiniMap.obj build\obj_x64\eterlib_d3d9ex\PythonApplication.obj | findstr /C:"8664 machine (x64)"
if errorlevel 1 exit /b 1

if exist "build\obj_x64\test_d3d9ex_x64.exe" goto :have_d3d9ex_exe
cl.exe /nologo /std:c++20 /EHsc /MT /O2 /W3 /DWIN32 /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS /Iextern\common\include /Iextern\client\include /Iclient\src /Iclient\src\EterLib tests\test_d3d9ex_x64.cpp /Fe:build\obj_x64\test_d3d9ex_x64.exe /Fo:build\obj_x64\test_d3d9ex_x64.obj /link /MACHINE:X64 d3d9.lib user32.lib gdi32.lib
if errorlevel 1 exit /b 1
:have_d3d9ex_exe

dumpbin.exe /headers build\obj_x64\test_d3d9ex_x64.exe | findstr /C:"8664 machine (x64)" /C:"PE32+"
if errorlevel 1 exit /b 1

build\obj_x64\test_d3d9ex_x64.exe
if errorlevel 1 exit /b 1

echo [6/6] Compiling and linking Unified Server/Client x64 SDK Suite (Crypto++ 8.9.0, LZO 2.10, DevIL 1.8.0, Boost 1.92.0, RapidJSON 1.1.0, fmt 12.2.0, msl 1.2.1)...
if not exist "build\obj_x64\unified_sdks" mkdir "build\obj_x64\unified_sdks"
cl.exe /nologo /std:c++20 /utf-8 /EHsc /MT /O2 /W3 /DWIN32 /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS /D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING /DMT2_USE_PYTHON3=1 /DMT2_PYTHON_MODULE_ONLY=1 /I"%PY311_DIR%\include" /Iextern\common\include /Iextern\client\include /Iclient\src /Iclient\src\EterPack /Iclient\src\EterBase /c ^
    client\src\UserInterface\HashUtils.cpp ^
    tests\test_unified_sdks_x64.cpp ^
    /Fo:build\obj_x64\unified_sdks\
if not %errorlevel%==0 exit /b 1

if exist "build\obj_x64\unified_sdks\Hardware.obj" goto :have_unified_objs
cl.exe /nologo /std:c++20 /utf-8 /EHsc /MT /O2 /W3 /DWIN32 /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS /D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING /DMT2_USE_PYTHON3=1 /DMT2_USE_MINIAUDIO=1 /I"%PY311_DIR%\include" /Iextern\common\include /Iextern\client\include /Iclient\src /Iclient\src\EterLib /Iclient\src\EterBase /Iclient\src\EterPack /Iclient\src\EterImageLib /Iclient\src\GameLib /Iclient\src\SpeedTreeLib /c ^
    client\src\UserInterface\MarkImage.cpp ^
    client\src\UserInterface\Hardware.cpp ^
    /Fo:build\obj_x64\unified_sdks\
if not %errorlevel%==0 exit /b 1
:have_unified_objs

dumpbin.exe /headers build\obj_x64\unified_sdks\MarkImage.obj build\obj_x64\unified_sdks\Hardware.obj build\obj_x64\unified_sdks\HashUtils.obj build\obj_x64\mileslib_prod\cipher.obj | findstr /C:"8664 machine (x64)"
if not %errorlevel%==0 exit /b 1

link.exe /nologo /MACHINE:X64 /LIBPATH:extern\client\lib /LIBPATH:"%PY311_DIR%\libs" /NODEFAULTLIB:d3dx9.lib /OUT:build\obj_x64\test_unified_sdks_x64.exe ^
    build\obj_x64\unified_sdks\test_unified_sdks_x64.obj ^
    build\obj_x64\unified_sdks\MarkImage.obj ^
    build\obj_x64\unified_sdks\Hardware.obj ^
    build\obj_x64\unified_sdks\HashUtils.obj ^
    build\obj_x64\mileslib_prod\cipher.obj ^
    build\obj_x64\mileslib_prod\EterPackPolicy_CSHybridCrypt.obj ^
    build\obj_x64\mileslib_prod\lzo.obj ^
    build\obj_x64\mileslib_prod\tea.obj ^
    build\obj_x64\mileslib_prod\CRC32.obj ^
    build\obj_x64\mileslib_prod\FileBase.obj ^
    build\obj_x64\mileslib_prod\FileDir.obj ^
    build\obj_x64\mileslib_prod\Utils.obj ^
    build\obj_x64\mileslib_prod\Random.obj ^
    build\obj_x64\mileslib_prod\Debug.obj ^
    build\obj_x64\mileslib_prod\Stl.obj ^
    build\obj_x64\mileslib_prod\Timer.obj ^
    extern\client\lib\cryptlib-x64.lib ^
    extern\client\lib\lzo2_x64.lib ^
    extern\client\lib\devil_x64.lib ^
    ole32.lib user32.lib gdi32.lib winmm.lib advapi32.lib
if not %errorlevel%==0 exit /b 1

dumpbin.exe /headers build\obj_x64\test_unified_sdks_x64.exe | findstr /C:"8664 machine (x64)" /C:"PE32+"
if not %errorlevel%==0 exit /b 1

build\obj_x64\test_unified_sdks_x64.exe
if not %errorlevel%==0 exit /b 1
