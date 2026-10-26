# ==============================================================================
# Metin2 MT2009 - Gerçek Canlı Veritabanı Test Çalıştırıcısı (Windows PowerShell)
# ==============================================================================

param (
    [string]$HostName = $(if ($env:MT2_TEST_HOST) { $env:MT2_TEST_HOST } else { "127.0.0.1" }),
    [int]$Port = $(if ($env:MT2_TEST_PORT) { [int]$env:MT2_TEST_PORT } else { 3306 }),
    [string]$User = $(if ($env:MT2_TEST_USER) { $env:MT2_TEST_USER } else { "mt2test" }),
    [string]$Password = $(if ($env:MT2_TEST_PASSWORD) { $env:MT2_TEST_PASSWORD } else { "" }),
    [string]$Database = $(if ($env:MT2_TEST_DB) { $env:MT2_TEST_DB } else { "mt2_testdb" }),
    [string]$Engine = $(if ($env:MT2_TEST_ENGINE) { $env:MT2_TEST_ENGINE } else { "MariaDB 12.3 LTS" })
)

$ErrorActionPreference = "Stop"

$RootDir = (Resolve-Path "$PSScriptRoot\..").Path
$BuildDir = "$RootDir\tests\build"
$SqlDir = "$RootDir\server\src\libsql"
$CoreDir = "$RootDir\server\src\libthecore"
$CommonDir = "$RootDir\server\src\common"
$ExternCommonDir = "$RootDir\extern\common\include"
$ExternServerDir = "$RootDir\extern\server\include"

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
}

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host " METİN2 MT2009 - CANLI VERİTABANI TEST ÇALIŞTIRICISI" -ForegroundColor Cyan
Write-Host " Hedef Uç Nokta: $HostName`:$Port / DB: $Database" -ForegroundColor Gray
Write-Host " Kullanıcı: $User / Parola: [KORUMALI / LOGLANMAZ]" -ForegroundColor Gray
Write-Host " Motor: $Engine / Bağlayıcı: MariaDB Connector/C (libmariadb)" -ForegroundColor Gray
Write-Host "==================================================" -ForegroundColor Cyan

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

$CommonIncludes = @(
    "-I$SqlDir",
    "-I$CoreDir\include",
    "-I$CommonDir",
    "-I$ExternCommonDir",
    "-I$ExternServerDir",
    "-I$ExternServerDir\mysql"
)

# MariaDB Connector/C (libmariadb) Kütüphane ve Başlık Belirleme
$ExeName = "$BuildDir\test_live_mariadb.exe"
$IncArgs = @("-I$ExternServerDir\mysql")
$LibArgs = @("-L$RootDir\extern\platform\windows-x64\mingw-ucrt-gcc16\lib", "-lmariadb")

Write-Host "Derleniyor: $ExeName..." -ForegroundColor Yellow
& $Gxx @CommonFlags @IncArgs @CommonIncludes "$SqlDir\AsyncSQL.cpp" "$SqlDir\Semaphore.cpp" "$RootDir\tests\test_live_db.cpp" "$BuildDir\libthecore.a" @LibArgs -lws2_32 -lsecur32 -lcrypt32 -lshlwapi -o "$ExeName"
if ($LASTEXITCODE -ne 0) { throw "test_live_db derleme/bağlama hatası!" }

Write-Host "Çalıştırılıyor: $ExeName..." -ForegroundColor Green
$env:Path = "$RootDir\build\windows-gcc-ucrt\tests;$RootDir\server\files\share\bin;$RootDir\server\src\db;$RootDir\server\src\game;$env:Path"
$env:MT2_TEST_HOST = $HostName
$env:MT2_TEST_PORT = "$Port"
$env:MT2_TEST_USER = $User
$env:MT2_TEST_PASSWORD = $Password
$env:MT2_TEST_DB = $Database
$env:MT2_TEST_ENGINE = $Engine
$env:RUN_LIVE_DB_TESTS = "1"
if (-not $env:MT2_TEST_SSL_CA) { $env:MT2_TEST_SSL_CA = "$RootDir\scratch\test_ssl\ca-cert.pem" }
if (-not $env:MT2_TEST_INVALID_SSL_CA) { $env:MT2_TEST_INVALID_SSL_CA = "$RootDir\scratch\test_ssl\invalid-ca-cert.pem" }

& "$ExeName"
$res = $LASTEXITCODE
if ($res -ne 0) {
    throw "Canlı veritabanı testinde başarısız assertion (Çıkış kodu: $res)!"
}

$GameExe = "$RootDir\server\src\game\game_r41023.exe"
if (-not (Test-Path $GameExe)) {
    $GameExe = "$RootDir\server\files\share\bin\game_r41023.exe"
}
if (-not (Test-Path $GameExe)) {
    $GameExe = "$RootDir\build\windows-gcc-ucrt\server\src\game\game_r41023.exe"
}
if (Test-Path $GameExe) {
    Write-Host "`nÇalıştırılıyor (Gerçek CItemShopManager Akışı): $GameExe --test-itemshop..." -ForegroundColor Green
    & "$GameExe" --test-itemshop --host "$HostName" --port "$Port" --user "$User" --password "$Password" --db "$Database"
    $gameRes = $LASTEXITCODE
    if ($gameRes -ne 0) {
        throw "Gerçek CItemShopManager canlı testinde başarısız assertion (Çıkış kodu: $gameRes)!"
    }
}

Write-Host "`n[BAŞARILI] Canlı veritabanı testi tamamlandı!" -ForegroundColor Green
exit 0
