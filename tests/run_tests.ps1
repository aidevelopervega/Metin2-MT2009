# ==============================================================================
# Metin2 MT2009 - Otomatik Modül Test Çalıştırıcısı (Windows PowerShell)
# ==============================================================================

$ErrorActionPreference = "Stop"

$RootDir = (Resolve-Path "$PSScriptRoot\..").Path
$BuildDir = "$RootDir\tests\build"
$ServerDir = "$RootDir\mt2009 - FULL SOURCE\Server Source"

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
        $env:Path = "$WinLibsBin;" + $env:Path
    } else {
        Write-Error "GCC (g++.exe) bulunamadı!"
    }
}

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host " METİN2 MT2009 - REGRESYON VE MODÜL TESTLERİ" -ForegroundColor Cyan
Write-Host " Derleyici: $Gxx" -ForegroundColor Gray
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
$PolyDir = "$ServerDir\Server\libpoly"
$ExternDir = "$ServerDir\Extern"

$PolyIncludes = @(
    "-I$PolyDir",
    "-I$ExternDir\include"
)

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
if ($LASTEXITCODE -ne 0) { throw "test_libpoly testi başarısız oldu!" }

# ------------------------------------------------------------------------------
# 2. LIBGAME DERLEME VE TESTİ
# ------------------------------------------------------------------------------
Write-Host "`n[2/2] libgame derleniyor ve test ediliyor..." -ForegroundColor Yellow
$GameDir = "$ServerDir\Server\libgame"
$CoreDir = "$ServerDir\Server\libthecore"
$CommonDir = "$ServerDir\Server\common"
$ExternDir = "$ServerDir\Extern"

$GameIncludes = @(
    "-I$GameDir\include",
    "-I$CoreDir\include",
    "-I$CommonDir",
    "-I$ExternDir\include"
)

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
& $Gxx @CommonFlags @GameIncludes "$RootDir\tests\test_libgame.cpp" "$GameLib" -o "$GameTestExe"
if ($LASTEXITCODE -ne 0) { throw "test_libgame bağlama hatası" }

Write-Host "--> test_libgame.exe çalıştırılıyor..." -ForegroundColor Green
& "$GameTestExe"
if ($LASTEXITCODE -ne 0) { throw "test_libgame testi başarısız oldu!" }

Write-Host "`n==================================================" -ForegroundColor Cyan
Write-Host " [BAŞARILI] TÜM MODÜL VE REGRESYON TESTLERİ GEÇTİ!" -ForegroundColor Green
Write-Host "==================================================" -ForegroundColor Cyan
