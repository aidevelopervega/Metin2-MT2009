# ==============================================================================
# Metin2 MT2009 - Gerçek Canlı Veritabanı Test Çalıştırıcısı (Windows PowerShell)
# ==============================================================================

param (
    [string]$HostName = $(if ($env:MT2_TEST_HOST) { $env:MT2_TEST_HOST } else { "127.0.0.1" }),
    [int]$Port = $(if ($env:MT2_TEST_PORT) { [int]$env:MT2_TEST_PORT } else { 3306 }),
    [string]$User = $(if ($env:MT2_TEST_USER) { $env:MT2_TEST_USER } else { "mt2test" }),
    [string]$Password = $(if ($env:MT2_TEST_PASSWORD) { $env:MT2_TEST_PASSWORD } else { "" }),
    [string]$Database = $(if ($env:MT2_TEST_DB) { $env:MT2_TEST_DB } else { "mt2_testdb" }),
    [string]$Engine = $(if ($env:MT2_TEST_ENGINE) { $env:MT2_TEST_ENGINE } else { "Auto-Detect" }),
    [string]$ConnectorType = "mariadb" # "mariadb" veya "mysql"
)

$ErrorActionPreference = "Stop"

$RootDir = (Resolve-Path "$PSScriptRoot\..").Path
$BuildDir = "$RootDir\tests\build"
$ServerDir = "$RootDir\mt2009 - FULL SOURCE\Server Source"
$SqlDir = "$ServerDir\Server\libsql"
$CoreDir = if (Test-Path "$RootDir\server\src\libthecore") { "$RootDir\server\src\libthecore" } else { "$ServerDir\Server\libthecore" }
$CommonDir = "$ServerDir\Server\common"
$ExternDir = "$ServerDir\Extern"

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
Write-Host " Motor: $Engine / Bağlayıcı Türü: $ConnectorType" -ForegroundColor Gray
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
    "-I$ExternDir\include"
)

# Kütüphane ve Başlık Belirleme
if ($ConnectorType -eq "mysql") {
    $ExeName = "$BuildDir\test_live_mysql.exe"
    $IncArgs = @("-I$RootDir\scratch\isolated_db\mysql_inc", "-I$RootDir\scratch\isolated_db\mysql-8.4.11-winx64\include")
    $LibArgs = @("-L$RootDir\scratch\isolated_db\mysql-8.4.11-winx64\lib", "-lmysql")
} else {
    $ExeName = "$BuildDir\test_live_mariadb.exe"
    $IncArgs = @("-I$RootDir\scratch\isolated_db\mariadb-11.4.13-winx64\include", "-I$RootDir\scratch\isolated_db\mariadb-11.4.13-winx64\include\mysql")
    $LibArgs = @("-L$RootDir\scratch\isolated_db\mariadb-11.4.13-winx64\lib", "-lmariadb")
}

Write-Host "Derleniyor: $ExeName..." -ForegroundColor Yellow
& $Gxx @CommonFlags @IncArgs @CommonIncludes "$SqlDir\AsyncSQL.cpp" "$SqlDir\Semaphore.cpp" "$RootDir\tests\test_live_db.cpp" "$BuildDir\libthecore.a" @LibArgs -lws2_32 -lsecur32 -lcrypt32 -lshlwapi -o "$ExeName"
if ($LASTEXITCODE -ne 0) { throw "test_live_db derleme/bağlama hatası!" }

Write-Host "Çalıştırılıyor: $ExeName..." -ForegroundColor Green
$env:Path = "$RootDir\scratch\isolated_db\mariadb-11.4.13-winx64\lib;$RootDir\scratch\isolated_db\mysql-8.4.11-winx64\lib;$RootDir\scratch\isolated_db\mysql-8.4.11-winx64\bin;$env:Path"
$env:MT2_TEST_HOST = $HostName
$env:MT2_TEST_PORT = "$Port"
$env:MT2_TEST_USER = $User
$env:MT2_TEST_PASSWORD = $Password
$env:MT2_TEST_DB = $Database
$env:MT2_TEST_ENGINE = $Engine
$env:RUN_LIVE_DB_TESTS = "1"

if ($ConnectorType -eq "mysql") {
    $env:MT2_TEST_SSL_CA = "$RootDir\scratch\isolated_db\data_mysql84\ca.pem"
    $env:MT2_TEST_INVALID_SSL_CA = "$RootDir\scratch\test_ssl\invalid-ca-cert.pem"
} else {
    $env:MT2_TEST_SSL_CA = "$RootDir\scratch\test_ssl\ca-cert.pem"
    $env:MT2_TEST_INVALID_SSL_CA = "$RootDir\scratch\test_ssl\invalid-ca-cert.pem"
}

& "$ExeName"
$res = $LASTEXITCODE
if ($res -ne 0) {
    throw "Canlı veritabanı testinde başarısız assertion (Çıkış kodu: $res)!"
}

Write-Host "`n[BAŞARILI] Canlı veritabanı testi tamamlandı!" -ForegroundColor Green
exit 0
