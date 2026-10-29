param(
    [int]$Port = 3307,
    [string]$MariaDbBinDir = "",
    [string]$BuildDir = "",
    [int]$SpoolRepeatCount = 1,
    [switch]$SkipProtocolSmoke,
    [switch]$IncludeSecurityAndBackup
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..")).Path

if ($Port -eq 3306) {
    throw "SAFETY VIOLATION: Port 3306 is reserved for persistent serverfiles MariaDB. Use an isolated port such as 3307."
}

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $repoRoot "build\windows-gcc-ucrt"
}

function Resolve-MariaDbBinDir([string]$explicitDir) {
    $candidates = @()
    if (![string]::IsNullOrWhiteSpace($explicitDir)) { $candidates += $explicitDir }
    if (![string]::IsNullOrWhiteSpace($env:MT2_DB_BIN_DIR)) { $candidates += $env:MT2_DB_BIN_DIR }
    $candidates += (Join-Path $repoRoot "scratch\handoff\isolated_db\mariadb-12.3.3-winx64\bin")

    foreach ($cand in $candidates) {
        if ((Test-Path (Join-Path $cand "mariadbd.exe")) -and (Test-Path (Join-Path $cand "mariadb.exe"))) {
            return (Resolve-Path $cand).Path
        }
    }
    $cmd = Get-Command mariadbd.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($cmd) {
        return (Split-Path -Parent $cmd.Source)
    }
    throw "Cannot locate MariaDB 12.3 LTS bin directory (mariadbd.exe / mariadb.exe). Pass -MariaDbBinDir or set MT2_DB_BIN_DIR."
}

function New-RandomToken([int]$len = 16) {
    $chars = "abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789"
    $bytes = New-Object byte[] $len
    $rng = [System.Security.Cryptography.RandomNumberGenerator]::Create()
    $rng.GetBytes($bytes)
    $sb = New-Object System.Text.StringBuilder
    for ($i = 0; $i -lt $len; $i++) {
        [void]$sb.Append($chars[$bytes[$i] % $chars.Length])
    }
    return $sb.ToString()
}

$binDir = Resolve-MariaDbBinDir $MariaDbBinDir
$winLibsBin = "C:\Users\oyun\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin"
if ((Test-Path $winLibsBin) -and ($env:PATH -notlike "*$winLibsBin*")) {
    $env:PATH = "$winLibsBin;$env:PATH"
}
if ($env:PATH -notlike "*$binDir*") {
    $env:PATH = "$binDir;$env:PATH"
}

$mariadbdExe = Join-Path $binDir "mariadbd.exe"
$mariadbExe = Join-Path $binDir "mariadb.exe"
$mysqlAdminExe = Join-Path $binDir "mysqladmin.exe"
$installDbExe = Join-Path $binDir "mariadb-install-db.exe"
if (!(Test-Path $installDbExe)) {
    $installDbExe = Join-Path $binDir "mysql_install_db.exe"
}
$pluginDir = (Resolve-Path (Join-Path $binDir "..\lib\plugin")).Path

$testLiveDbExe = Join-Path $BuildDir "tests\test_live_db.exe"
$testDbCacheIntExe = Join-Path $BuildDir "tests\test_db_cache_integration.exe"
$smokeClientExe = Join-Path $BuildDir "tests\test_server_smoke_client.exe"
$gameExe = Join-Path $repoRoot "server\src\game\game_r41023.exe"
$dbExe = Join-Path $repoRoot "server\src\db\db_r41023.exe"

foreach ($reqBin in @($testLiveDbExe, $testDbCacheIntExe, $smokeClientExe, $gameExe, $dbExe)) {
    if (!(Test-Path $reqBin)) {
        throw "Required binary missing: $reqBin. Build first with .\tests\run_limited.ps1 -Build"
    }
}

$workRoot = Join-Path $repoRoot ("scratch\kademe_c_live_" + $Port)
$protectedDataDir = (Join-Path $repoRoot "server\files\mysql_data").ToLowerInvariant()
if ($workRoot.ToLowerInvariant().StartsWith($protectedDataDir)) {
    throw "SAFETY VIOLATION: Isolated work directory cannot overlap server\files\mysql_data."
}

$dataDir = Join-Path $workRoot "data"
$tlsDir = Join-Path $workRoot "tls"
$logDir = Join-Path $workRoot "logs"
$backupDir = Join-Path $workRoot "backups"

$adminPass = "Adm!" + (New-RandomToken 18)
$rtPass = "Rt!" + (New-RandomToken 18)
$logPass = "Lg!" + (New-RandomToken 18)
$bkPass = "Bk!" + (New-RandomToken 18)
$smokeAcctPass = "Smk!" + (New-RandomToken 8) # 12 chars (within 6..16 client PASSWD_MAX_LEN)

$sfDir = Join-Path $repoRoot "server\files"
foreach ($dllDir in @(
    (Join-Path $BuildDir "tests"),
    (Join-Path $sfDir "share\bin"),
    (Join-Path $repoRoot "server\src\db"),
    (Join-Path $repoRoot "server\src\game")
)) {
    if ((Test-Path $dllDir) -and ($env:PATH -notlike "*$dllDir*")) {
        $env:PATH = "$dllDir;$env:PATH"
    }
}
$protectedConfigs = @(
    (Join-Path $sfDir "db\conf.txt"),
    (Join-Path $sfDir "auth\CONFIG"),
    (Join-Path $sfDir "chs\ch11\CONFIG")
)
$configBackups = @{}
$configHashesBefore = @{}
foreach ($cfgPath in $protectedConfigs) {
    if (Test-Path $cfgPath) {
        $configBackups[$cfgPath] = [System.IO.File]::ReadAllBytes($cfgPath)
        $configHashesBefore[$cfgPath] = (Get-FileHash -Algorithm SHA256 $cfgPath).Hash
    }
}

$mariadbdProc = $null
$startedProcs = @()

function Invoke-MySqlRoot([string]$sql, [string]$db = "") {
    $prevEap = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $argsList = @("--protocol=TCP", "--skip-ssl", "-h", "127.0.0.1", "-P", "$Port", "-u", "root", "-N", "-s", "-e", $sql)
        if (![string]::IsNullOrWhiteSpace($db)) { $argsList += $db }
        $out = & $mariadbExe @argsList 2>&1
        if ($LASTEXITCODE -ne 0) {
            throw "MariaDB root query failed (exit $LASTEXITCODE): $($out | Out-String)"
        }
        return ($out | Out-String).Trim()
    } finally {
        $ErrorActionPreference = $prevEap
    }
}

function Invoke-MySqlFileRoot([string]$filePath, [string]$db) {
    $prevEap = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $strictMode = "STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION"
        $normPath = (Resolve-Path $filePath).Path.Replace('\', '/')
        $out = & $mariadbExe --protocol=TCP --skip-ssl --default-character-set=utf8mb4 -h 127.0.0.1 -P $Port -u root $db -e "SET SESSION sql_mode='$strictMode'; SOURCE $normPath;" 2>&1
        if ($LASTEXITCODE -ne 0) {
            throw "Loading SQL file '$filePath' into '$db' failed (exit $LASTEXITCODE): $($out | Out-String)"
        }
    } finally {
        $ErrorActionPreference = $prevEap
    }
}

function Wait-TcpPort([int]$p, [int]$maxRetries = 60) {
    for ($i = 0; $i -lt $maxRetries; $i++) {
        $tc = New-Object System.Net.Sockets.TcpClient
        try {
            $tc.Connect("127.0.0.1", $p)
            if ($tc.Connected) { return $true }
        } catch {} finally {
            $tc.Dispose()
        }
        Start-Sleep -Milliseconds 500
    }
    return $false
}

try {
    Write-Host "=== [0/6] Provisioning Isolated MariaDB 12.3 LTS on 127.0.0.1:$Port ===" -ForegroundColor Cyan
    if (Test-Path $workRoot) {
        Remove-Item -Recurse -Force $workRoot -ErrorAction SilentlyContinue
    }
    New-Item -ItemType Directory -Force -Path $dataDir, $tlsDir, $logDir, $backupDir | Out-Null
    Remove-Item -Force (Join-Path $repoRoot "test_live_sql_log_spool*") -ErrorAction SilentlyContinue

    $opensslExe = $null
    $opensslCmd = Get-Command openssl.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($opensslCmd) {
        $opensslExe = $opensslCmd.Source
    } else {
        foreach ($candSsl in @(
            "C:\Program Files\Git\mingw64\bin\openssl.exe",
            "C:\Program Files\Git\usr\bin\openssl.exe",
            (Join-Path $winLibsBin "openssl.exe")
        )) {
            if (Test-Path $candSsl) { $opensslExe = $candSsl; break }
        }
    }
    if (!$opensslExe) {
        throw "Cannot locate openssl.exe for MariaDB TLS certificate generation."
    }

    $prevEap = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    & $opensslExe req -x509 -newkey rsa:2048 -nodes -keyout (Join-Path $tlsDir "ca-key.pem") -out (Join-Path $tlsDir "ca-cert.pem") -days 2 -subj "/CN=Metin2 Test CA" *> $null
    & $opensslExe req -newkey rsa:2048 -nodes -keyout (Join-Path $tlsDir "server-key.pem") -out (Join-Path $tlsDir "server-req.pem") -subj "/CN=127.0.0.1" *> $null
    & $opensslExe x509 -req -in (Join-Path $tlsDir "server-req.pem") -CA (Join-Path $tlsDir "ca-cert.pem") -CAkey (Join-Path $tlsDir "ca-key.pem") -CAcreateserial -out (Join-Path $tlsDir "server-cert.pem") -days 2 *> $null
    & $opensslExe req -x509 -newkey rsa:2048 -nodes -keyout (Join-Path $tlsDir "invalid-ca-key.pem") -out (Join-Path $tlsDir "invalid-ca-cert.pem") -days 2 -subj "/CN=Untrusted Fake CA" *> $null

    & $installDbExe "--datadir=$dataDir" *> $null
    if ($LASTEXITCODE -ne 0) {
        throw "mariadb-install-db failed with exit code $LASTEXITCODE"
    }
    $ErrorActionPreference = $prevEap

    $auditLogPath = Join-Path $logDir "mariadb_audit.log"
    $mariadbdArgs = @(
        "--datadir=$dataDir",
        "--port=$Port",
        "--bind-address=127.0.0.1",
        "--local-infile=0",
        "--symbolic-links=0",
        "--character-set-server=utf8mb4",
        "--collation-server=utf8mb4_general_ci",
        "--sql-mode=STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION",
        "--max-allowed-packet=64M",
        "--plugin-dir=$pluginDir",
        "--plugin-load-add=server_audit.dll",
        "--server-audit=FORCE_PLUS_PERMANENT",
        "--server-audit-events=CONNECT,QUERY_DDL,QUERY_DCL,QUERY_DML_NO_SELECT",
        "--server-audit-output-type=FILE",
        "--server-audit-file-path=$auditLogPath",
        "--server-audit-file-rotate-size=104857600",
        "--server-audit-file-rotations=90",
        "--server-audit-logging=ON",
        "--ssl-ca=$(Join-Path $tlsDir 'ca-cert.pem')",
        "--ssl-cert=$(Join-Path $tlsDir 'server-cert.pem')",
        "--ssl-key=$(Join-Path $tlsDir 'server-key.pem')"
    )

    $mariadbdProc = Start-Process -FilePath $mariadbdExe -ArgumentList $mariadbdArgs -WorkingDirectory $workRoot -PassThru -WindowStyle Hidden
    $ready = $false
    for ($w = 0; $w -lt 50; $w++) {
        Start-Sleep -Milliseconds 300
        $pEap = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        & $mysqlAdminExe --protocol=TCP --skip-ssl -h 127.0.0.1 -P $Port -u root ping --silent *> $null
        $pingCode = $LASTEXITCODE
        $ErrorActionPreference = $pEap
        if ($pingCode -eq 0) { $ready = $true; break }
    }
    if (!$ready) {
        throw "Isolated mariadbd failed to become ready on port $Port"
    }
    Write-Host "  -> Isolated MariaDB 12.3 LTS listening on 127.0.0.1:$Port (PID $($mariadbdProc.Id))" -ForegroundColor Green

    $bootstrapSql = @"
CREATE DATABASE IF NOT EXISTS mt2_testdb CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
CREATE DATABASE IF NOT EXISTS account CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
CREATE DATABASE IF NOT EXISTS common  CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
CREATE DATABASE IF NOT EXISTS log     CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
CREATE DATABASE IF NOT EXISTS player  CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
CREATE DATABASE IF NOT EXISTS world   CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;

CREATE USER IF NOT EXISTS 'mt2test'@'127.0.0.1' IDENTIFIED BY '$adminPass';
CREATE USER IF NOT EXISTS 'mt2test'@'localhost' IDENTIFIED BY '$adminPass';
GRANT CREATE, ALTER, DROP, INDEX, LOCK TABLES, TRIGGER, EVENT, SELECT, INSERT, UPDATE, DELETE ON mt2_testdb.* TO 'mt2test'@'127.0.0.1', 'mt2test'@'localhost';
GRANT CREATE, ALTER, DROP, INDEX, LOCK TABLES, TRIGGER, EVENT, SELECT, INSERT, UPDATE, DELETE ON account.*    TO 'mt2test'@'127.0.0.1', 'mt2test'@'localhost';
GRANT CREATE, ALTER, DROP, INDEX, LOCK TABLES, TRIGGER, EVENT, SELECT, INSERT, UPDATE, DELETE ON common.*     TO 'mt2test'@'127.0.0.1', 'mt2test'@'localhost';
GRANT CREATE, ALTER, DROP, INDEX, LOCK TABLES, TRIGGER, EVENT, SELECT, INSERT, UPDATE, DELETE ON log.*        TO 'mt2test'@'127.0.0.1', 'mt2test'@'localhost';
GRANT CREATE, ALTER, DROP, INDEX, LOCK TABLES, TRIGGER, EVENT, SELECT, INSERT, UPDATE, DELETE ON player.*     TO 'mt2test'@'127.0.0.1', 'mt2test'@'localhost';
GRANT CREATE, ALTER, DROP, INDEX, LOCK TABLES, TRIGGER, EVENT, SELECT, INSERT, UPDATE, DELETE ON world.*      TO 'mt2test'@'127.0.0.1', 'mt2test'@'localhost';

CREATE USER IF NOT EXISTS 'mt2server'@'127.0.0.1' IDENTIFIED BY '$rtPass';
CREATE USER IF NOT EXISTS 'mt2server'@'localhost' IDENTIFIED BY '$rtPass';
GRANT SELECT, INSERT, UPDATE, DELETE ON mt2_testdb.* TO 'mt2server'@'127.0.0.1', 'mt2server'@'localhost';
GRANT SELECT, INSERT, UPDATE, DELETE ON account.*    TO 'mt2server'@'127.0.0.1', 'mt2server'@'localhost';
GRANT SELECT, INSERT, UPDATE, DELETE ON common.*     TO 'mt2server'@'127.0.0.1', 'mt2server'@'localhost';
GRANT SELECT, INSERT, UPDATE, DELETE ON player.*     TO 'mt2server'@'127.0.0.1', 'mt2server'@'localhost';
GRANT SELECT                         ON world.*      TO 'mt2server'@'127.0.0.1', 'mt2server'@'localhost';

CREATE USER IF NOT EXISTS 'mt2log'@'127.0.0.1' IDENTIFIED BY '$logPass';
CREATE USER IF NOT EXISTS 'mt2log'@'localhost' IDENTIFIED BY '$logPass';
GRANT SELECT, INSERT, UPDATE, DELETE ON log.* TO 'mt2log'@'127.0.0.1', 'mt2log'@'localhost';

CREATE USER IF NOT EXISTS 'mt2backup'@'127.0.0.1' IDENTIFIED BY '$bkPass';
CREATE USER IF NOT EXISTS 'mt2backup'@'localhost' IDENTIFIED BY '$bkPass';
GRANT SELECT, SHOW VIEW, TRIGGER, LOCK TABLES, EVENT ON mt2_testdb.* TO 'mt2backup'@'127.0.0.1', 'mt2backup'@'localhost';
GRANT SELECT, SHOW VIEW, TRIGGER, LOCK TABLES, EVENT ON account.*    TO 'mt2backup'@'127.0.0.1', 'mt2backup'@'localhost';
GRANT SELECT, SHOW VIEW, TRIGGER, LOCK TABLES, EVENT ON common.*     TO 'mt2backup'@'127.0.0.1', 'mt2backup'@'localhost';
GRANT SELECT, SHOW VIEW, TRIGGER, LOCK TABLES, EVENT ON log.*        TO 'mt2backup'@'127.0.0.1', 'mt2backup'@'localhost';
GRANT SELECT, SHOW VIEW, TRIGGER, LOCK TABLES, EVENT ON player.*     TO 'mt2backup'@'127.0.0.1', 'mt2backup'@'localhost';
GRANT SELECT, SHOW VIEW, TRIGGER, LOCK TABLES, EVENT ON world.*      TO 'mt2backup'@'127.0.0.1', 'mt2backup'@'localhost';
FLUSH PRIVILEGES;
"@
    [void](Invoke-MySqlRoot $bootstrapSql)

    Write-Host "=== [1/6] Loading 5 Schemas + Running 4 Migrations Twice (2x Idempotency) ===" -ForegroundColor Cyan
    foreach ($s in @("account", "common", "log", "player", "world")) {
        Invoke-MySqlFileRoot (Join-Path $repoRoot "sql\$s.sql") $s
    }
    $migrations = @(
        @{ File = "rev47_account_security_upgrade.sql"; Db = "account" },
        @{ File = "rev51_halloween_hair_attr_migration.sql"; Db = "world" },
        @{ File = "rev52_log_ikarusshop_itemshop_tables.sql"; Db = "log" },
        @{ File = "20261027_x64_protocol_and_itemshop_journal.sql"; Db = "account" }
    )
    for ($pass = 1; $pass -le 2; $pass++) {
        foreach ($m in $migrations) {
            Invoke-MySqlFileRoot (Join-Path $repoRoot "sql\migrations\$($m.File)") $m.Db
        }
        Write-Host "  -> Migration pass $pass/2 completed cleanly."
    }

    $engineStats = Invoke-MySqlRoot "SELECT COUNT(*), SUM(CASE WHEN ENGINE='InnoDB' THEN 1 ELSE 0 END), SUM(CASE WHEN ENGINE='MyISAM' THEN 1 ELSE 0 END) FROM information_schema.TABLES WHERE TABLE_SCHEMA IN ('account','common','log','player','world') AND TABLE_TYPE='BASE TABLE';"
    Write-Host "  -> 5-Schema Base Tables (Total / InnoDB / MyISAM): $engineStats" -ForegroundColor Green

    $env:MT2_TEST_HOST = "127.0.0.1"
    $env:MT2_TEST_PORT = "$Port"
    $env:MT2_TEST_USER = "mt2test"
    $env:MT2_TEST_PASSWORD = $adminPass
    $env:MT2_TEST_DB = "mt2_testdb"
    $env:MT2_TEST_ENGINE = "MariaDB 12.3 LTS"
    $env:MT2_RUNTIME_USER = "mt2server"
    $env:MT2_RUNTIME_PASSWORD = $rtPass
    $env:MT2_LOG_DB_USER = "mt2log"
    $env:MT2_LOG_DB_PASS = $logPass
    $env:MT2_BACKUP_DB_USER = "mt2backup"
    $env:MT2_BACKUP_DB_PASS = $bkPass
    $env:MT2_TEST_SSL_CA = Join-Path $tlsDir "ca-cert.pem"
    $env:MT2_TEST_INVALID_SSL_CA = Join-Path $tlsDir "invalid-ca-cert.pem"
    $env:RUN_LIVE_DB_TESTS = "1"
    $env:MT2_DB_HOST = "127.0.0.1"
    $env:MT2_DB_PORT = "$Port"
    $env:MT2_DB_USER = "mt2test"
    $env:MT2_DB_PASS = $adminPass

    Write-Host "=== [2/6] Running test_live_db.exe ($SpoolRepeatCount consecutive run(s)) ===" -ForegroundColor Cyan
    for ($runIdx = 1; $runIdx -le $SpoolRepeatCount; $runIdx++) {
        $prevEap = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        $liveOut = & $testLiveDbExe 2>&1
        $liveExit = $LASTEXITCODE
        $ErrorActionPreference = $prevEap
        if ($liveExit -ne 0) {
            Write-Host ($liveOut | Out-String)
            throw "test_live_db.exe failed on run $runIdx/$SpoolRepeatCount with exit code $liveExit"
        }
        $mLive = $liveOut | Select-String -Pattern "PASSED|Passed" | Select-Object -Last 1
        $summaryLine = if ($mLive) { $mLive.Line.Trim() } else { "PASS (exit 0)" }
        Write-Host "  -> Run $runIdx/${SpoolRepeatCount}: $summaryLine" -ForegroundColor Green
    }

    Write-Host "=== [3/6] Running Real ItemShop Integration (game_r41023.exe --test-itemshop) ===" -ForegroundColor Cyan
    $prevEap = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $isOut = & $gameExe --test-itemshop --host "127.0.0.1" --port "$Port" --user "mt2test" --password "$adminPass" --db "mt2_testdb" 2>&1
    $isExit = $LASTEXITCODE
    $ErrorActionPreference = $prevEap
    if ($isExit -ne 0) {
        Write-Host ($isOut | Out-String)
        throw "game_r41023.exe --test-itemshop failed with exit code $isExit"
    }
    $mIs = $isOut | Select-String -Pattern "PASSED|Passed" | Select-Object -Last 1
    $isSummary = if ($mIs) { $mIs.Line.Trim() } else { "PASS (exit 0)" }
    Write-Host "  -> $isSummary" -ForegroundColor Green

    Write-Host "=== [4/6] Running Live DB Cache Integration (test_db_cache_integration.exe) ===" -ForegroundColor Cyan
    $prevEap = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $cacheOut = & $testDbCacheIntExe 2>&1
    $cacheExit = $LASTEXITCODE
    $ErrorActionPreference = $prevEap
    if ($cacheExit -ne 0) {
        Write-Host ($cacheOut | Out-String)
        throw "test_db_cache_integration.exe failed with exit code $cacheExit"
    }
    $mCache = $cacheOut | Select-String -Pattern "PASSED|Passed" | Select-Object -Last 1
    $cacheSummary = if ($mCache) { $mCache.Line.Trim() } else { "PASS (exit 0)" }
    Write-Host "  -> $cacheSummary" -ForegroundColor Green

    if ($IncludeSecurityAndBackup) {
        Write-Host "=== [5/6] Running Backup HMAC-SHA256(v2), Audit Guard & POSIX Shell Verification ===" -ForegroundColor Cyan
        $gitSh = "C:\Program Files\Git\bin\sh.exe"
        if (Test-Path $gitSh) {
            foreach ($shRel in @(
                "server/scripts/freebsd/audit_guard.sh",
                "server/scripts/freebsd/backup_mariadb.sh",
                "server/scripts/freebsd/install.sh",
                "tests/run_freebsd_serverfiles_test.sh"
            )) {
                $shFull = Join-Path $repoRoot $shRel
                & $gitSh -n $shFull
                if ($LASTEXITCODE -ne 0) { throw "POSIX sh -n failed on $shRel" }
            }
            Write-Host "  -> POSIX sh -n syntax checks (4 FreeBSD scripts): PASS" -ForegroundColor Green

            $agDir = Join-Path $workRoot "audit_guard_test"
            New-Item -ItemType Directory -Force -Path "$agDir\data", "$agDir\archive", "$agDir\sf\chs\ch11" | Out-Null
            $agLog = ($agDir -replace '\\', '/') + "/data/mariadb_audit.log"
            $agArch = ($agDir -replace '\\', '/') + "/archive"
            $agAlert = Join-Path $agDir "alerts.log"
            $sampleRot = "20261029 12:00:00,mt2,mt2server,127.0.0.1,1,1,QUERY,account,'INSERT INTO account (login,password,social_id,email) VALUES (''u1'',''`$argon2id`$v=19`$m=65536,t=3,p=1`$c2FsdA`$b2xk'',''7654321'',''old@example.com'')',0`n"
            [System.IO.File]::WriteAllText("$agDir\data\mariadb_audit.log", "20261029 12:01:00,mt2,mt2server,127.0.0.1,1,2,QUERY,player,'UPDATE player SET gold=100',0`n", [System.Text.Encoding]::ASCII)
            [System.IO.File]::WriteAllText("$agDir\data\mariadb_audit.log.001", $sampleRot, [System.Text.Encoding]::ASCII)
            $agScript = (Join-Path $repoRoot "server/scripts/freebsd/audit_guard.sh") -replace '\\', '/'
            $prevEap = $ErrorActionPreference
            $ErrorActionPreference = "Continue"
            & $gitSh $agScript --enforce-perms --audit-log $agLog --archive-dir $agArch --alert-file $agAlert *> $null
            $agExit = $LASTEXITCODE
            $ErrorActionPreference = $prevEap
            if ($agExit -ne 0) { throw "audit_guard.sh --enforce-perms failed with exit code $agExit" }
            $archFile = Get-ChildItem -Path "$agDir\archive" -Filter "mariadb_audit.log.001.*" | Select-Object -First 1
            if (!$archFile -or (Get-Content $archFile.FullName -Raw) -notmatch "\`$argon2id\`$\[REDACTED\]") {
                throw "audit_guard.sh redaction check failed"
            }
            Write-Host "  -> audit_guard.sh redaction & archive verification: PASS" -ForegroundColor Green
        }

        $env:MT2_DB_HOST = "127.0.0.1"
        $env:MT2_DB_PORT = "$Port"
        $env:MT2_DB_BIN_DIR = $binDir
        $env:MT2_BACKUP_DB_USER = "mt2backup"
        $env:MT2_BACKUP_DB_PASS = $bkPass
        $env:MT2_INIT_DB_USER = "mt2test"
        $env:MT2_INIT_DB_PASS = $adminPass
        $env:MT2_BACKUP_PASSPHRASE = (New-RandomToken 32)
        $env:MT2_BACKUP_HMAC_SECRET = (New-RandomToken 48)
        $env:MT2_BACKUP_RETENTION_COUNT = "5"

        $bkScript = Join-Path $repoRoot "server\scripts\windows\backup_mariadb.ps1"
        & powershell -NoProfile -ExecutionPolicy Bypass -File $bkScript -Action Backup -BackupDir $backupDir -Databases @("player")
        if ($LASTEXITCODE -ne 0) {
            throw "backup_mariadb.ps1 Backup failed with exit code $LASTEXITCODE"
        }
        $encFile = Get-ChildItem -Path $backupDir -Filter "mt2_backup_*.sql.gz.enc" | Select-Object -First 1
        if (!$encFile) { throw "backup_mariadb.ps1 did not produce .sql.gz.enc archive" }
        $shaFile = "$($encFile.FullName).sha256"
        if ((Get-Content $shaFile -Raw) -notmatch "HMAC-SHA256\(v2\)=[0-9a-f]{64}") {
            throw "Backup manifest missing HMAC-SHA256(v2) tag"
        }

        $forgedEnc = Join-Path $backupDir "forged_backup.sql.gz.enc"
        $forgedSha = "${forgedEnc}.sha256"
        $forgedBytes = [System.IO.File]::ReadAllBytes($encFile.FullName)
        $forgedBytes[$forgedBytes.Length - 1] = $forgedBytes[$forgedBytes.Length - 1] -bxor 0xFF
        [System.IO.File]::WriteAllBytes($forgedEnc, $forgedBytes)
        $forgedShaHex = (Get-FileHash -Path $forgedEnc -Algorithm SHA256).Hash.ToLowerInvariant()
        $origHmacLine = (Get-Content $shaFile | Where-Object { $_ -match "^HMAC-SHA256\(v2\)=" } | Select-Object -First 1)
        [System.IO.File]::WriteAllText($forgedSha, "$forgedShaHex  forged_backup.sql.gz.enc`n$origHmacLine`n", [System.Text.Encoding]::ASCII)
        $forgedRejected = $false
        try {
            & $bkScript -Action Restore -BackupDir $backupDir -RestoreArchive $forgedEnc -TargetDatabase "player_forged_test" *> $null
        } catch {
            if ($_.Exception.Message -match "HMAC-SHA256\(v2\) verification FAILED") {
                $forgedRejected = $true
            }
        }
        Remove-Item -Force $forgedEnc, $forgedSha -ErrorAction SilentlyContinue
        if (!$forgedRejected) {
            throw "Forged archive + recomputed SHA-256 manifest was NOT rejected by HMAC-SHA256(v2)"
        }

        & $mariadbExe --protocol=TCP --skip-ssl -h 127.0.0.1 -P $Port -u root -e "DROP DATABASE IF EXISTS player_restore_verify; CREATE DATABASE player_restore_verify CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci; GRANT ALL PRIVILEGES ON player_restore_verify.* TO 'mt2test'@'127.0.0.1', 'mt2test'@'localhost'; FLUSH PRIVILEGES;"
        & powershell -NoProfile -ExecutionPolicy Bypass -File $bkScript -Action Restore -BackupDir $backupDir -RestoreArchive $encFile.FullName -TargetDatabase "player_restore_verify"
        if ($LASTEXITCODE -ne 0) {
            throw "backup_mariadb.ps1 Restore failed with exit code $LASTEXITCODE"
        }
        & $mariadbExe --protocol=TCP --skip-ssl -h 127.0.0.1 -P $Port -u root -e "DROP DATABASE IF EXISTS player_restore_verify;"
        Write-Host "  -> Keyed HMAC-SHA256(v2) Backup, Tamper Rejection & Restore: PASS" -ForegroundColor Green
    } else {
        Write-Host "=== [5/6] Security & Backup Extended Stage Skipped (pass -IncludeSecurityAndBackup to run) ===" -ForegroundColor DarkGray
    }

    if (!$SkipProtocolSmoke) {
        Write-Host "=== [6/6] Running Isolated Serverfiles Protocol Framing & >2^32 Auth/Game/Replay Smoke ===" -ForegroundColor Cyan
        Copy-Item -Force $dbExe (Join-Path $sfDir "share\bin\db_r41023.exe")
        Copy-Item -Force $gameExe (Join-Path $sfDir "share\bin\game_r41023.exe")

        foreach ($entry in $configBackups.GetEnumerator()) {
            $text = [System.Text.Encoding]::ASCII.GetString($entry.Value)
            if ($entry.Key.EndsWith("conf.txt")) {
                $text = $text -replace '(?m)^(SQL_ACCOUNT\s*=\s*"127\.0\.0\.1\s+account\s+)\S+\s+\S+(\s+)\d+"\s*$', "`${1}mt2server $rtPass`${2}$Port`""
                $text = $text -replace '(?m)^(SQL_COMMON\s*=\s*"127\.0\.0\.1\s+common\s+)\S+\s+\S+(\s+)\d+"\s*$', "`${1}mt2server $rtPass`${2}$Port`""
                $text = $text -replace '(?m)^(SQL_PLAYER\s*=\s*"127\.0\.0\.1\s+player\s+)\S+\s+\S+(\s+)\d+"\s*$', "`${1}mt2server $rtPass`${2}$Port`""
                $text = $text -replace '(?m)^(SQL_HOTBACKUP\s*=\s*"127\.0\.0\.1\s+player\s+)\S+\s+\S+(\s+)\d+"\s*$', "`${1}mt2server $rtPass`${2}$Port`""
                $text = $text -replace '(?m)^(SQL_WORLD\s*=\s*"127\.0\.0\.1\s+world\s+)\S+\s+\S+(\s+)\d+"\s*$', "`${1}mt2server $rtPass`${2}$Port`""
                $text = $text -replace '(?m)^(SQL_LOG\s*=\s*"127\.0\.0\.1\s+log\s+)\S+\s+\S+(\s+)\d+"\s*$', "`${1}mt2log $logPass`${2}$Port`""
                $text = $text -replace '(?m)^PLAYER_ID_START\s*=\s*\d+\s*$', "PLAYER_ID_START = 5000000001"
                $text = $text -replace '(?m)^ITEM_ID_RANGE\s*=\s*\d+\s+\d+\s*$', "ITEM_ID_RANGE = 5000000001 5100000000"
            } else {
                $text = $text -replace '(?m)^((?:ACCOUNT|COMMON|PLAYER|SQL_HOTBACKUP|SQL_WORLD)_SQL:\s*127\.0\.0\.1\s+)\S+\s+\S+(\s+\S+\s+)\d+\s*$', "`${1}mt2server $rtPass`${2}$Port"
                $text = $text -replace '(?m)^(LOG_SQL:\s*127\.0\.0\.1\s+)\S+\s+\S+(\s+log\s+)\d+\s*$', "`${1}mt2log $logPass`${2}$Port"
            }
            [System.IO.File]::WriteAllText($entry.Key, $text, [System.Text.Encoding]::ASCII)
        }

        foreach ($svcSub in @("db", "auth", "chs\ch11")) {
            foreach ($lf in @("pid", "pid.win", "stop_signal", "syserr.txt", "syslog.txt", "stdout.log", "stderr.log")) {
                $lp = Join-Path $sfDir "$svcSub\$lf"
                if (Test-Path $lp) { Remove-Item -Force $lp -ErrorAction SilentlyContinue }
            }
        }

        [void](Invoke-MySqlRoot "UPDATE common.locale SET mValue='poland' WHERE mKey='LOCALE';")
        $smokeHwidPlain = "SMOKETEST_HWID1"
        $smokeHwidHex = "5BE8C5A4CAFB1014505B16C5D68D28A9"

        Push-Location (Join-Path $sfDir "db")
        try {
            $prevEap = $ErrorActionPreference
            $ErrorActionPreference = "Continue"
            $acctOut = ($smokeAcctPass | & (Join-Path $sfDir "share\bin\db_r41023.exe") --create-account "win64hero" "1234567" $smokeHwidPlain 2>&1 | Out-String).Trim()
            $acctExit = $LASTEXITCODE
            $ErrorActionPreference = $prevEap
            if ($acctExit -ne 0) {
                throw "db_r41023.exe --create-account failed: $acctOut"
            }
        } finally {
            Pop-Location
        }
        [void](Invoke-MySqlRoot "UPDATE account.account SET id = 5000000001 WHERE login = 'win64hero'; ALTER TABLE player.player AUTO_INCREMENT = 5000000001; ALTER TABLE player.item AUTO_INCREMENT = 5000000001;")

        $dbRunProc = Start-Process -FilePath (Join-Path $sfDir "share\bin\db_r41023.exe") -WorkingDirectory (Join-Path $sfDir "db") -PassThru -WindowStyle Hidden
        $startedProcs += $dbRunProc
        if (!(Wait-TcpPort 18000 40)) {
            throw "db_r41023.exe failed to listen on port 18000"
        }

        $authCfgText = [System.IO.File]::ReadAllText((Join-Path $sfDir "auth\CONFIG"))
        $ch11CfgText = [System.IO.File]::ReadAllText((Join-Path $sfDir "chs\ch11\CONFIG"))
        $authPort = if ($authCfgText -match '(?m)^PORT:\s*(\d+)') { [int]$Matches[1] } else { 17105 }
        $ch11Port = if ($ch11CfgText -match '(?m)^PORT:\s*(\d+)') { [int]$Matches[1] } else { 28000 }

        $ch11RunProc = Start-Process -FilePath (Join-Path $sfDir "share\bin\game_r41023.exe") -WorkingDirectory (Join-Path $sfDir "chs\ch11") -PassThru -WindowStyle Hidden
        $startedProcs += $ch11RunProc
        $authRunProc = Start-Process -FilePath (Join-Path $sfDir "share\bin\game_r41023.exe") -WorkingDirectory (Join-Path $sfDir "auth") -PassThru -WindowStyle Hidden
        $startedProcs += $authRunProc

        if (!(Wait-TcpPort $authPort 60)) { throw "AUTH failed to listen on port $authPort" }
        if (!(Wait-TcpPort $ch11Port 60)) { throw "CH11 failed to listen on port $ch11Port" }

        $ch11Booted = $false
        $ch11Syslog = Join-Path $sfDir "chs\ch11\syslog.txt"
        for ($i = 0; $i -lt 90; $i++) {
            if (Test-Path $ch11Syslog) {
                if (Select-String -Path $ch11Syslog -Pattern "XXX Boot Call NPC" -ErrorAction SilentlyContinue) {
                    $ch11Booted = $true
                    break
                }
            }
            Start-Sleep -Milliseconds 500
        }
        if (!$ch11Booted) {
            throw "CH11 did not complete CInputDB::Boot within timeout"
        }
        Start-Sleep -Seconds 1

        $env:SMOKE_TEST_LOGIN = "win64hero"
        $env:SMOKE_TEST_PASS = $smokeAcctPass
        $env:SMOKE_REAL_SERVERFILES = "1"

        $prevEap = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        $pfOut = & $smokeClientExe --protocol-framing-audit $authPort $ch11Port "@ENV" "@ENV" $smokeHwidHex 2>&1
        $pfExit = $LASTEXITCODE
        $ErrorActionPreference = $prevEap
        if ($pfExit -ne 0) {
            Write-Host ($pfOut | Out-String)
            throw "Protocol Framing Audit failed with exit code $pfExit"
        }
        Write-Host "  -> Protocol Framing Audit: 10/10 PASS" -ForegroundColor Green

        $keyFile1 = Join-Path $workRoot "login_key_1.txt"
        $prevEap = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        $authKeyRaw = & $smokeClientExe --auth-login $authPort "@ENV" "@ENV" $smokeHwidHex $keyFile1 2>&1
        $authKeyExit = $LASTEXITCODE
        $ErrorActionPreference = $prevEap
        if ($authKeyExit -ne 0 -or !(Test-Path $keyFile1)) {
            throw "--auth-login failed: $($authKeyRaw | Out-String)"
        }
        $loginKey = (Get-Content $keyFile1 -Raw).Trim()

        $prevEap = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        $gameLoginRaw = & $smokeClientExe --game-login-key $ch11Port "@ENV" $loginKey 2>&1
        $gameLoginExit = $LASTEXITCODE
        $ErrorActionPreference = $prevEap
        $gameLoginStr = ($gameLoginRaw | Out-String).Trim()
        if ($gameLoginExit -ne 0 -or $gameLoginStr -notmatch "id=5000000001") {
            throw "--game-login-key failed or did not create character with id=5000000001: $gameLoginStr"
        }

        $prevEap = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        $replayRaw = & $smokeClientExe --game-login-key-expect-fail $ch11Port "@ENV" $loginKey 2>&1
        $replayExit = $LASTEXITCODE
        $ErrorActionPreference = $prevEap
        if ($replayExit -ne 0) {
            throw "--game-login-key-expect-fail failed: $($replayRaw | Out-String)"
        }
        Write-Host "  -> Single-use LoginKey Replay Rejection: PASS" -ForegroundColor Green

        $heroDbCheck = Invoke-MySqlRoot "SELECT CONCAT(a.id, ':', p.id, ':', p.name) FROM account.account a JOIN player.player p ON p.account_id = a.id WHERE a.login = 'win64hero';"
        if ($heroDbCheck -ne "5000000001:5000000001:SmokeHero") {
            throw "Expected 5000000001:5000000001:SmokeHero in MariaDB, got: $heroDbCheck"
        }
        Write-Host "  -> >2^32 Auth/Game/Quest Login & DB Persistence Verified: $heroDbCheck" -ForegroundColor Green
    }

    Write-Host "=== [OK] Kademe C Live Verification Completed Successfully ===" -ForegroundColor Green
} finally {
    foreach ($p in $startedProcs) {
        if ($p -and !$p.HasExited) {
            Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
            $p.WaitForExit(5000) | Out-Null
        }
    }
    if ($mariadbdProc -and !$mariadbdProc.HasExited) {
        $prevEap = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        & $mysqlAdminExe --protocol=TCP --skip-ssl -h 127.0.0.1 -P $Port -u root shutdown *> $null
        $ErrorActionPreference = $prevEap
        $mariadbdProc.WaitForExit(5000) | Out-Null
        if (!$mariadbdProc.HasExited) {
            Stop-Process -Id $mariadbdProc.Id -Force -ErrorAction SilentlyContinue
            $mariadbdProc.WaitForExit(5000) | Out-Null
        }
    }
    foreach ($entry in $configBackups.GetEnumerator()) {
        [System.IO.File]::WriteAllBytes($entry.Key, $entry.Value)
        $afterHash = (Get-FileHash -Algorithm SHA256 $entry.Key).Hash
        if ($afterHash -ne $configHashesBefore[$entry.Key]) {
            Write-Error "CRITICAL: Failed to restore exact SHA256 for $($entry.Key)"
        }
    }
    foreach ($k in @("MT2_TEST_HOST","MT2_TEST_PORT","MT2_TEST_USER","MT2_TEST_PASSWORD","MT2_TEST_DB","MT2_TEST_ENGINE","MT2_RUNTIME_USER","MT2_RUNTIME_PASSWORD","MT2_LOG_DB_USER","MT2_LOG_DB_PASS","MT2_BACKUP_DB_USER","MT2_BACKUP_DB_PASS","MT2_TEST_SSL_CA","MT2_TEST_INVALID_SSL_CA","RUN_LIVE_DB_TESTS","MT2_DB_HOST","MT2_DB_PORT","MT2_DB_USER","MT2_DB_PASS","SMOKE_TEST_LOGIN","SMOKE_TEST_PASS","SMOKE_REAL_SERVERFILES")) {
        Remove-Item "Env:\$k" -ErrorAction SilentlyContinue
    }
    if (Test-Path $workRoot) {
        Start-Sleep -Milliseconds 500
        Remove-Item -Recurse -Force $workRoot -ErrorAction SilentlyContinue
    }
}
