# ==============================================================================
# Metin2 MT2009 - Otomatik Modül Test Çalıştırıcısı (Windows PowerShell)
# ==============================================================================

$ErrorActionPreference = "Stop"

try {
    $RootDir = (Resolve-Path "$PSScriptRoot\..").Path
    $BuildDir = "$RootDir\tests\build"

    # GCC Araçlarını Tespit Et
    $GxxCmd = Get-Command "g++.exe" -ErrorAction SilentlyContinue
    $Gxx = if ($GxxCmd) { $GxxCmd.Source } else { $null }
    $ArCmd = Get-Command "ar.exe" -ErrorAction SilentlyContinue
    $Ar = if ($ArCmd) { $ArCmd.Source } else { $null }
    $RanlibCmd = Get-Command "ranlib.exe" -ErrorAction SilentlyContinue
    $Ranlib = if ($RanlibCmd) { $RanlibCmd.Source } else { $null }

    if (-not $Gxx) {
        $WinLibsBin = "C:\Users\oyun\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin"
        if (Test-Path "$WinLibsBin\g++.exe") {
            $Gxx = "$WinLibsBin\g++.exe"
            $Ar = "$WinLibsBin\ar.exe"
            $Ranlib = "$WinLibsBin\ranlib.exe"
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

    Write-Host "==================================================" -ForegroundColor Cyan
    Write-Host " METİN2 MT2009 - REGRESYON VE MODÜL TESTLERİ" -ForegroundColor Cyan
    Write-Host " Derleyici: $Gxx" -ForegroundColor Gray
    Write-Host " Çalışma Zamanı PATH: $(Split-Path -Parent $Gxx)" -ForegroundColor Gray
    Write-Host "==================================================" -ForegroundColor Cyan

    # Derleme Klasörünü Hazırla
    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
    }

    $CommonFlags = @(
        "-std=c++20",
        "-Wall",
        "-Wextra",
        "-O2",
        "-pipe",
        "-fno-exceptions",
        "-fno-rtti"
    )

    # ------------------------------------------------------------------------------
    # 1. LIBPOLY DERLEME VE TESTİ
    # ------------------------------------------------------------------------------
    Write-Host "`n[1/2] libpoly derleniyor ve test ediliyor..." -ForegroundColor Yellow
    $PolyDir = "$RootDir\server\src\libpoly"
    $ExternIncludes = @(
        "-I$RootDir\extern\common\include",
        "-I$RootDir\extern\server\include",
        "-I$RootDir\extern\server\include\mysql"
    )

    $PolyIncludes = @(
        "-I$PolyDir"
    ) + $ExternIncludes

    $PolySources = @("Base.cc", "Symbol.cc", "SymTable.cc", "Poly.cc")
    $PolyObjects = @()

    foreach ($src in $PolySources) {
        $obj = "$BuildDir\$([System.IO.Path]::GetFileNameWithoutExtension($src)).o"
        $PolyObjects += $obj
        & $Gxx @CommonFlags @PolyIncludes -c "$PolyDir\$src" -o "$obj"
        if ($LASTEXITCODE -ne 0) { throw "libpoly derleme hatası: $src" }
    }

    $PolyLib = "$BuildDir\libpoly.a"
    & $Ar cru "$PolyLib" $PolyObjects
    & $Ranlib "$PolyLib"

    $PolyTestExe = "$BuildDir\test_libpoly.exe"
    & $Gxx @CommonFlags @PolyIncludes "$RootDir\tests\test_libpoly.cpp" "$PolyLib" -o "$PolyTestExe"
    if ($LASTEXITCODE -ne 0) { throw "test_libpoly bağlama hatası" }

    Write-Host "--> test_libpoly.exe çalıştırılıyor..." -ForegroundColor Green
    & "$PolyTestExe"
    if ($LASTEXITCODE -ne 0) { throw "test_libpoly testi başarısız oldu (Çıkış kodu: $LASTEXITCODE)!" }

    # ------------------------------------------------------------------------------
    # 2. LIBTHECORE DERLEME VE TESTİ
    # ------------------------------------------------------------------------------
    Write-Host "`n[2/3] libthecore derleniyor ve test ediliyor..." -ForegroundColor Yellow
    $CoreDir = "$RootDir\server\src\libthecore"
    $CommonDir = "$RootDir\server\src\common"

    $CoreIncludes = @(
        "-I$CoreDir\include",
        "-I$CommonDir"
    ) + $ExternIncludes

    $CoreSources = @(
        "DES_table.c",
        "buffer.c",
        "des.c",
        "fdwatch.c",
        "gost.c",
        "hangul.c",
        "heart.c",
        "kstbl.c",
        "log.c",
        "main.c",
        "memcpy.c",
        "signal.c",
        "socket.c",
        "tea.c",
        "utils.c",
        "xdirent.c",
        "xgetopt.c",
        "xmd5.c"
    )

    $CoreObjects = @()
    foreach ($src in $CoreSources) {
        $obj = "$BuildDir\core_$([System.IO.Path]::GetFileNameWithoutExtension($src)).o"
        $CoreObjects += $obj
        & $Gxx @CommonFlags @CoreIncludes -c "$CoreDir\src\$src" -o "$obj"
        if ($LASTEXITCODE -ne 0) { throw "libthecore derleme hatası: $src" }
    }

    $CoreLib = "$BuildDir\libthecore.a"
    & $Ar cru "$CoreLib" $CoreObjects
    & $Ranlib "$CoreLib"

    $CoreTestExe = "$BuildDir\test_libthecore.exe"
    & $Gxx @CommonFlags @CoreIncludes "$RootDir\tests\test_libthecore.cpp" "$CoreLib" -lws2_32 -o "$CoreTestExe"
    if ($LASTEXITCODE -ne 0) { throw "test_libthecore bağlama hatası" }

    Write-Host "--> test_libthecore.exe çalıştırılıyor..." -ForegroundColor Green
    & "$CoreTestExe"
    if ($LASTEXITCODE -ne 0) { throw "test_libthecore testi başarısız oldu (Çıkış kodu: $LASTEXITCODE)!" }

    # ------------------------------------------------------------------------------
    # 3. LIBGAME DERLEME VE TESTİ (Gerçek libthecore.a ile bağlı)
    # ------------------------------------------------------------------------------
    Write-Host "`n[3/3] libgame derleniyor ve test ediliyor..." -ForegroundColor Yellow
    $GameDir = "$RootDir\server\src\libgame"

    $GameIncludes = @(
        "-I$GameDir\include",
        "-I$CoreDir\include",
        "-I$CommonDir"
    ) + $ExternIncludes

    $GameSources = @("grid.cc", "attribute.cc", "targa.cc")
    $GameObjects = @()

    foreach ($src in $GameSources) {
        $obj = "$BuildDir\game_$([System.IO.Path]::GetFileNameWithoutExtension($src)).o"
        $GameObjects += $obj
        & $Gxx @CommonFlags @GameIncludes -c "$GameDir\src\$src" -o "$obj"
        if ($LASTEXITCODE -ne 0) { throw "libgame derleme hatası: $src" }
    }

    $GameLib = "$BuildDir\libgame.a"
    & $Ar cru "$GameLib" $GameObjects
    & $Ranlib "$GameLib"

    $GameTestExe = "$BuildDir\test_libgame.exe"
    & $Gxx @CommonFlags @GameIncludes "$RootDir\tests\test_libgame.cpp" "$GameLib" "$CoreLib" -lws2_32 -o "$GameTestExe"
    if ($LASTEXITCODE -ne 0) { throw "test_libgame bağlama hatası" }

    Write-Host "--> test_libgame.exe çalıştırılıyor..." -ForegroundColor Green
    & "$GameTestExe"
    if ($LASTEXITCODE -ne 0) { throw "test_libgame testi başarısız oldu (Çıkış kodu: $LASTEXITCODE)!" }

    # ------------------------------------------------------------------------------
    # 4. LIBSQL DERLEME VE TESTİ
    # ------------------------------------------------------------------------------
    Write-Host "`n[4/4] libsql derleniyor ve test ediliyor..." -ForegroundColor Yellow
    $SqlDir = "$RootDir\server\src\libsql"

    $SqlIncludes = @(
        "-I$SqlDir",
        "-I$CoreDir\include",
        "-I$CommonDir"
    ) + $ExternIncludes

    $SqlSources = @("AsyncSQL.cpp", "Semaphore.cpp", "Statement.cpp", "Tellwait.cpp")
    $SqlObjects = @()

    foreach ($src in $SqlSources) {
        $obj = "$BuildDir\sql_$([System.IO.Path]::GetFileNameWithoutExtension($src)).o"
        $SqlObjects += $obj
        & $Gxx @CommonFlags @SqlIncludes -c "$SqlDir\$src" -o "$obj"
        if ($LASTEXITCODE -ne 0) { throw "libsql derleme hatası: $src" }
    }

    $SqlLib = "$BuildDir\libsql.a"
    & $Ar cru "$SqlLib" $SqlObjects
    & $Ranlib "$SqlLib"

    $SqlTestExe = "$BuildDir\test_libsql.exe"
    & $Gxx @CommonFlags @SqlIncludes "$RootDir\tests\test_libsql.cpp" "$SqlLib" "$CoreLib" -lws2_32 -o "$SqlTestExe"
    if ($LASTEXITCODE -ne 0) { throw "test_libsql bağlama hatası" }

    Write-Host "--> test_libsql.exe çalıştırılıyor..." -ForegroundColor Green
    & "$SqlTestExe"
    if ($LASTEXITCODE -ne 0) { throw "test_libsql testi başarısız oldu (Çıkış kodu: $LASTEXITCODE)!" }

    # ------------------------------------------------------------------------------
    # 5. DB CACHE VE VERİ GÜVENLİĞİ TESTİ (Senaryo A - H)
    # ------------------------------------------------------------------------------
    Write-Host "`n[5/5] db cache ve veri güvenliği testleri derleniyor ve test ediliyor..." -ForegroundColor Yellow
    $DbCacheIncludes = @(
        "-I$CoreDir\include",
        "-I$CommonDir",
        "-I$SqlDir"
    ) + $ExternIncludes

    $DbCacheTestExe = "$BuildDir\test_db_cache.exe"
    & $Gxx @CommonFlags @DbCacheIncludes "$RootDir\tests\test_db_cache.cpp" "$CoreLib" -lws2_32 -o "$DbCacheTestExe"
    if ($LASTEXITCODE -ne 0) { throw "test_db_cache bağlama hatası" }

    Write-Host "--> test_db_cache.exe çalıştırılıyor..." -ForegroundColor Green
    & "$DbCacheTestExe"
    if ($LASTEXITCODE -ne 0) { throw "test_db_cache testi başarısız oldu (Çıkış kodu: $LASTEXITCODE)!" }

    # ------------------------------------------------------------------------------
    # 6. DB CACHE / ACK GERÇEK ENTEGRASYON TESTİ (Senaryo 1 - 7)
    # ------------------------------------------------------------------------------
    Write-Host "`n[6/7] DB cache / ACK gerçek entegrasyon testleri çalıştırılıyor..." -ForegroundColor Yellow
    & "$RootDir\tests\run_db_integration_tests.ps1"
    if ($LASTEXITCODE -ne 0) { throw "run_db_integration_tests testi başarısız oldu (Çıkış kodu: $LASTEXITCODE)!" }

    # ------------------------------------------------------------------------------
    # 7. WIRE PROTOCOL VE BINARY UYUMLULUK TESTİ (32 assertion)
    # ------------------------------------------------------------------------------
    Write-Host "`n[7/7] Wire protocol ve binary paket uyumluluğu test ediliyor..." -ForegroundColor Yellow
    $WireTestExe = "$BuildDir\test_packet_wire.exe"
    $WireIncludes = @(
        "-I$RootDir",
        "-I$RootDir\server\src\game\src",
        "-I$RootDir\server\src\common",
        "-I$RootDir\server\src\libthecore\include"
    ) + $ExternIncludes

    & $Gxx @CommonFlags @WireIncludes "$RootDir\tests\test_packet_wire.cpp" -o "$WireTestExe"
    if ($LASTEXITCODE -ne 0) { throw "test_packet_wire derleme hatası" }

    Write-Host "--> test_packet_wire.exe çalıştırılıyor..." -ForegroundColor Green
    & "$WireTestExe"
    if ($LASTEXITCODE -ne 0) { throw "test_packet_wire testi başarısız oldu (Çıkış kodu: $LASTEXITCODE)!" }

    Write-Host "`n==================================================" -ForegroundColor Cyan
    Write-Host " [BAŞARILI] TÜM MODÜL VE REGRESYON TESTLERİ GEÇTİ!" -ForegroundColor Green
    Write-Host "==================================================" -ForegroundColor Cyan
    exit 0

} catch {
    Write-Host "`n[HATA] $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
