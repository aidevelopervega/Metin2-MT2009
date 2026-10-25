# ==============================================================================
# Metin2 MT2009 - Gerçek DB Cache / ACK Entegrasyon Test Çalıştırıcısı (Windows)
# ==============================================================================

$ErrorActionPreference = "Stop"

try {
    $RootDir = (Resolve-Path "$PSScriptRoot\..").Path
    $DbSrcDir = "$RootDir\server\src\db\src"
    $BuildDir = "$RootDir\tests\build"
    $DbWinDir = "$BuildDir\db_win"

    # GCC Araçlarını Tespit Et
    $GxxCmd = Get-Command "g++.exe" -ErrorAction SilentlyContinue
    $Gxx = if ($GxxCmd) { $GxxCmd.Source } else { $null }

    if (-not $Gxx) {
        $WinLibsBin = "C:\Users\oyun\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin"
        if (Test-Path "$WinLibsBin\g++.exe") {
            $Gxx = "$WinLibsBin\g++.exe"
            $env:Path = "$WinLibsBin;$env:Path"
        } else {
            throw "GCC (g++.exe) bulunamadı!"
        }
    } else {
        $GxxDir = Split-Path -Parent $Gxx
        if ($env:Path -notlike "*$GxxDir*") {
            $env:Path = "$GxxDir;$env:Path"
        }
    }

    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
    }
    if (-not (Test-Path $DbWinDir)) {
        New-Item -ItemType Directory -Path $DbWinDir -Force | Out-Null
    }

    $IncDirs = @(
        "-I$DbSrcDir",
        "-I$RootDir\server\src\libthecore\include",
        "-I$RootDir\server\src\libsql",
        "-I$RootDir\server\src\libpoly",
        "-I$RootDir\server\src\libgame\include",
        "-I$RootDir\server\src\libgame\src",
        "-I$RootDir\server\src\common",
        "-I$RootDir\extern\common\include",
        "-I$RootDir\extern\common\include\boost",
        "-I$RootDir\extern\server\include",
        "-I$RootDir\extern\server\include\mysql"
    )

    $Flags = @(
        "-std=c++20",
        "-Wall",
        "-Wextra",
        "-O2",
        "-pipe",
        "-fno-rtti",
        "-fno-exceptions"
    )

    # DB nesne dosyalarının varlığını kontrol et, yoksa derle
    $DbSources = @(
        "ClientManager.cpp", "ClientManagerPlayer.cpp", "ClientManagerLogin.cpp",
        "ClientManagerBoot.cpp", "ClientManagerEventFlag.cpp", "ClientManagerGuild.cpp",
        "ClientManagerHorseName.cpp", "ClientManagerIkarusShop.cpp", "ClientManagerParty.cpp",
        "Cache.cpp", "Config.cpp", "CsvReader.cpp", "DBManager.cpp",
        "GuildManager.cpp", "HB.cpp", "IkarusShopCache.cpp", "ItemAwardManager.cpp",
        "ItemIDRangeManager.cpp", "Lock.cpp", "LoginData.cpp", "Marriage.cpp",
        "Monarch.cpp", "MoneyLog.cpp", "NetBase.cpp", "Peer.cpp", "PeerBase.cpp",
        "PrivManager.cpp", "ProtoReader.cpp", "version.cpp"
    )

    # Header/flag dependency stamp: objects must be rebuilt when any shared header
    # (db, libsql, libthecore, common) or this build script (flags/include order) changes.
    $DepRoots = @($DbSrcDir, "$RootDir\server\src\libsql", "$RootDir\server\src\libthecore\include", "$RootDir\server\src\common")
    $NewestDep = (Get-ChildItem -Path $DepRoots -Include *.h, *.hpp -Recurse -File |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1).LastWriteTime
    $ScriptTime = (Get-Item $PSCommandPath).LastWriteTime
    if ($ScriptTime -gt $NewestDep) { $NewestDep = $ScriptTime }

    foreach ($src in $DbSources) {
        $objName = [System.IO.Path]::GetFileNameWithoutExtension($src) + ".o"
        $objPath = "$DbWinDir\$objName"
        $srcPath = "$DbSrcDir\$src"
        $stale = $true
        if (Test-Path $objPath) {
            $objTime = (Get-Item $objPath).LastWriteTime
            $stale = ((Get-Item $srcPath).LastWriteTime -gt $objTime) -or ($NewestDep -gt $objTime)
        }
        if ($stale) {
            Write-Host "Compiling $src..." -ForegroundColor Yellow
            & $Gxx @Flags @IncDirs -c $srcPath -o $objPath
            if ($LASTEXITCODE -ne 0) { throw "Derleme hatası: $src" }
        }
    }

    $src = "$RootDir\tests\test_db_cache_integration.cpp"
    $obj = "$BuildDir\test_db_cache_integration.o"
    $target = "$BuildDir\test_db_cache_integration.exe"

    Write-Host "Compiling test_db_cache_integration.cpp..." -ForegroundColor Yellow
    & $Gxx @Flags @IncDirs -c $src -o $obj
    if ($LASTEXITCODE -ne 0) { throw "test_db_cache_integration.cpp derleme hatası" }

    Write-Host "Linking test_db_cache_integration.exe..." -ForegroundColor Yellow
    $objs = Get-ChildItem -Path $DbWinDir -Filter "*.o" | Where-Object { $_.Name -ne "Main.o" } | ForEach-Object { $_.FullName }
    $thecore = "$BuildDir\libthecore.a"
    $sql = "$BuildDir\libsql.a"
    $poly = "$BuildDir\libpoly.a"
    $game = "$BuildDir\libgame.a"
    $mariadb = if (Test-Path "$RootDir\extern\platform\windows-x64\mingw-ucrt-gcc16\lib\libmariadb.a") {
        "$RootDir\extern\platform\windows-x64\mingw-ucrt-gcc16\lib\libmariadb.a"
    } elseif (Test-Path "$RootDir\extern\platform\windows-x64\lib\libmariadb.a") {
        "$RootDir\extern\platform\windows-x64\lib\libmariadb.a"
    } else {
        throw "libmariadb.a bulunamadı!"
    }

    & $Gxx @Flags $obj @objs $thecore $sql $poly $game $mariadb -lws2_32 -lbcrypt -lsecur32 -lcrypt32 -lshlwapi -o $target
    if ($LASTEXITCODE -ne 0) { throw "test_db_cache_integration.exe bağlama hatası" }

    Write-Host "Running test_db_cache_integration.exe..." -ForegroundColor Green
    $env:Path = "$RootDir\build\windows-gcc-ucrt\tests;$RootDir\server\files\share\bin;$RootDir\server\src\db;$RootDir\server\src\game;$env:Path"
    & $target
    if ($LASTEXITCODE -ne 0) { throw "Entegrasyon testi başarısız oldu (Çıkış kodu: $LASTEXITCODE)!" }

    exit 0
} catch {
    Write-Host "`n[HATA] $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
