param(
    [string]$TargetDir = "",
    [string]$EnvFile = "",
    [string]$RepoRoot = "",
    [switch]$InitSql,
    [switch]$SkipConfig
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if (![string]::IsNullOrWhiteSpace($RepoRoot)) {
    $repoRoot = (Resolve-Path $RepoRoot).Path
} elseif (Test-Path (Join-Path $scriptDir "..\..\..\CMakeLists.txt")) {
    $repoRoot = (Resolve-Path (Join-Path $scriptDir "..\..\..")).Path
} elseif (Test-Path (Join-Path $scriptDir "..\..\CMakeLists.txt")) {
    $repoRoot = (Resolve-Path (Join-Path $scriptDir "..\..")).Path
} else {
    throw "Cannot locate repository root from $scriptDir. Pass -RepoRoot explicitly."
}

if ([string]::IsNullOrWhiteSpace($TargetDir)) {
    $TargetDir = Join-Path $repoRoot "server\files"
}

function Load-EnvMap([string]$path) {
    $map = @{}
    if ([string]::IsNullOrWhiteSpace($path) -or !(Test-Path $path)) {
        return $map
    }
    foreach ($line in Get-Content $path) {
        $trimmed = $line.Trim()
        if ($trimmed.Length -eq 0 -or $trimmed.StartsWith("#")) { continue }
        $idx = $trimmed.IndexOf("=")
        if ($idx -gt 0) {
            $k = $trimmed.Substring(0, $idx).Trim()
            $v = $trimmed.Substring($idx + 1).Trim()
            $map[$k] = $v
        }
    }
    return $map
}

function Get-ConfigValue([hashtable]$map, [string]$key, [string]$defaultVal) {
    $envVal = [Environment]::GetEnvironmentVariable($key)
    if (![string]::IsNullOrWhiteSpace($envVal)) { return $envVal }
    if ($map.ContainsKey($key) -and ![string]::IsNullOrWhiteSpace($map[$key])) { return $map[$key] }
    return $defaultVal
}

function Protect-SensitiveFile([string]$filePath) {
    if (Test-Path $filePath) {
        $currentUser = [System.Security.Principal.WindowsIdentity]::GetCurrent().Name
        & icacls $filePath /inheritance:r /grant:r "${currentUser}:(R,W)" "SYSTEM:(F)" "Administrators:(F)" | Out-Null
    }
}

Write-Host "=== [1/4] Preparing Windows x64 Serverfiles Directory: $TargetDir ==="
New-Item -ItemType Directory -Force -Path (Join-Path $TargetDir "share\bin") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $TargetDir "db\log\archive") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $TargetDir "auth\log\archive") | Out-Null
foreach ($ch in @("ch11","ch12","ch13","ch99","ch00","ch01")) {
    New-Item -ItemType Directory -Force -Path (Join-Path $TargetDir "chs\$ch\log\archive") | Out-Null
}

$shareArchive = Join-Path $repoRoot "server\serverfiles_share.tar.xz"
$questObjDir = Join-Path $TargetDir "share\locale\poland\quest\object"
if (!(Test-Path $questObjDir)) {
    if (!(Test-Path $shareArchive)) {
        throw "Missing serverfiles share archive: $shareArchive"
    }
    Write-Host "  -> Extracting $shareArchive into $TargetDir..."
    & tar -xJf $shareArchive --strip-components=3 -C $TargetDir
    if ($LASTEXITCODE -ne 0) {
        throw "tar extraction of $shareArchive failed with exit code $LASTEXITCODE"
    }
}

Write-Host "=== [2/4] Packaging x64 Executables, Runtime DLLs, and Shared Assets ==="
$dbExe = Join-Path $repoRoot "server\src\db\db_r41023.exe"
$gameExe = Join-Path $repoRoot "server\src\game\game_r41023.exe"

foreach ($bin in @($dbExe, $gameExe)) {
    if (!(Test-Path $bin)) {
        throw "Required x64 binary not found: $bin (build with cmake --build --preset windows-gcc-ucrt first)"
    }
}

Copy-Item -Force $dbExe (Join-Path $TargetDir "share\bin\db_r41023.exe")
Copy-Item -Force $gameExe (Join-Path $TargetDir "share\bin\game_r41023.exe")
$staleLua50 = Join-Path $TargetDir "share\bin\game_r41023_lua50.exe"
if (Test-Path $staleLua50) {
    Remove-Item -Force $staleLua50
}

$qcCandidates = @(
    (Join-Path $repoRoot "build\windows-gcc-ucrt\server\src\game\src\quest\qc.exe"),
    (Join-Path $repoRoot "server\src\game\src\quest\qc.exe")
)
foreach ($qcBin in $qcCandidates) {
    if (Test-Path $qcBin) {
        Copy-Item -Force $qcBin (Join-Path $TargetDir "share\locale\poland\quest\qc.exe")
        break
    }
}

$dllCandidates = @(
    (Join-Path $repoRoot "server\src\db\libmariadb.dll"),
    (Join-Path $repoRoot "server\src\game\libmariadb.dll"),
    (Join-Path $repoRoot "extern\platform\windows-ucrt64\bin\libmariadb.dll")
)
foreach ($dll in $dllCandidates) {
    if (Test-Path $dll) {
        Copy-Item -Force $dll (Join-Path $TargetDir "share\bin\libmariadb.dll")
        break
    }
}

foreach ($s in @("start.ps1", "stop.ps1", "status.ps1", "install.ps1")) {
    $srcScript = Join-Path $scriptDir $s
    if (Test-Path $srcScript) {
        Copy-Item -Force $srcScript (Join-Path $TargetDir $s)
    }
}

Copy-Item -Force (Join-Path $TargetDir "share\bin\db_r41023.exe") (Join-Path $TargetDir "db\db_r41023.exe")
if (Test-Path (Join-Path $TargetDir "share\bin\libmariadb.dll")) {
    Copy-Item -Force (Join-Path $TargetDir "share\bin\libmariadb.dll") (Join-Path $TargetDir "db\libmariadb.dll")
}

$cmdSource = Join-Path $TargetDir "chs\ch01\CMD"
$markTgaSource = Join-Path $TargetDir "chs\ch01\mark\mark_0.tga"

$gameDirs = @(Join-Path $TargetDir "auth")
foreach ($ch in @("ch11","ch12","ch13","ch99","ch00","ch01")) {
    $gameDirs += (Join-Path $TargetDir "chs\$ch")
}

foreach ($gdir in $gameDirs) {
    Copy-Item -Force (Join-Path $TargetDir "share\bin\game_r41023.exe") (Join-Path $gdir "game_r41023.exe")
    if (Test-Path (Join-Path $TargetDir "share\bin\libmariadb.dll")) {
        Copy-Item -Force (Join-Path $TargetDir "share\bin\libmariadb.dll") (Join-Path $gdir "libmariadb.dll")
    }
    foreach ($linkName in @("data", "locale", "package")) {
        $linkPath = Join-Path $gdir $linkName
        $targetPath = Join-Path $TargetDir "share\$linkName"
        if (!(Test-Path $linkPath)) {
            New-Item -ItemType Junction -Path $linkPath -Target $targetPath | Out-Null
        }
    }
    if ((Test-Path $cmdSource) -and ($gdir -ne (Join-Path $TargetDir "chs\ch01"))) {
        Copy-Item -Force $cmdSource (Join-Path $gdir "CMD")
    }
    $markDir = Join-Path $gdir "mark"
    New-Item -ItemType Directory -Force -Path $markDir | Out-Null
    if ((Test-Path $markTgaSource) -and ($gdir -ne (Join-Path $TargetDir "chs\ch01"))) {
        Copy-Item -Force $markTgaSource (Join-Path $markDir "mark_0.tga")
    }
    $markIdx = Join-Path $markDir "mark_index"
    if (!(Test-Path $markIdx)) {
        [System.IO.File]::WriteAllText($markIdx, "", [System.Text.Encoding]::ASCII)
    }
}

$defaultEnvTarget = Join-Path $TargetDir "serverfiles.env"
$exampleEnv = Join-Path $repoRoot "server\config\serverfiles.env.example"
if ([string]::IsNullOrWhiteSpace($EnvFile)) {
    if (Test-Path $defaultEnvTarget) {
        $EnvFile = $defaultEnvTarget
    } elseif (Test-Path (Join-Path $repoRoot "server\config\serverfiles.env")) {
        $EnvFile = Join-Path $repoRoot "server\config\serverfiles.env"
    } else {
        if (Test-Path $exampleEnv) {
            Copy-Item -Force $exampleEnv $defaultEnvTarget
            Protect-SensitiveFile $defaultEnvTarget
        }
        $EnvFile = $defaultEnvTarget
    }
}

$cfg = Load-EnvMap $EnvFile
$dbHost = Get-ConfigValue $cfg "MT2_DB_HOST" "127.0.0.1"
$dbPort = Get-ConfigValue $cfg "MT2_DB_PORT" "3306"
$dbUser = Get-ConfigValue $cfg "MT2_DB_USER" "mt2server"
$dbPass = Get-ConfigValue $cfg "MT2_DB_PASS" ""
$logDbUser = Get-ConfigValue $cfg "MT2_LOG_DB_USER" $dbUser
$logDbPass = Get-ConfigValue $cfg "MT2_LOG_DB_PASS" $dbPass
if ([string]::IsNullOrWhiteSpace($logDbPass) -or $logDbPass.StartsWith("__CHANGE_ME_")) {
    $logDbUser = $dbUser
    $logDbPass = $dbPass
}
$initDbUser = Get-ConfigValue $cfg "MT2_INIT_DB_USER" $dbUser
$initDbPass = Get-ConfigValue $cfg "MT2_INIT_DB_PASS" $dbPass
if ([string]::IsNullOrWhiteSpace($initDbPass) -or $initDbPass.StartsWith("__CHANGE_ME_")) {
    $initDbUser = $dbUser
    $initDbPass = $dbPass
}
$bindIp = Get-ConfigValue $cfg "MT2_BIND_IP" "127.0.0.1"
$dbAddr = Get-ConfigValue $cfg "MT2_DB_ADDR" "127.0.0.1"
$dbListenPort = Get-ConfigValue $cfg "MT2_DB_LISTEN_PORT" "18000"
$authPort = Get-ConfigValue $cfg "MT2_AUTH_PORT" "17105"
$authP2pPort = Get-ConfigValue $cfg "MT2_AUTH_P2P_PORT" "19555"
$adminIp = Get-ConfigValue $cfg "MT2_ADMINPAGE_IP" "127.0.0.1"
$adminPass = Get-ConfigValue $cfg "MT2_ADMINPAGE_PASSWORD" ""
$serverKey = Get-ConfigValue $cfg "MT2_SERVER_KEY" ""
$localeSvc = Get-ConfigValue $cfg "MT2_LOCALE_SERVICE" "poland"

if (!$SkipConfig) {
    Write-Host "=== [3/4] Generating Secure Runtime Configurations (ACL-Restricted) ==="
    if ([string]::IsNullOrWhiteSpace($dbPass) -or $dbPass.StartsWith("__CHANGE_ME_")) {
        throw "MT2_DB_PASS is not configured or still uses a placeholder in $EnvFile. Set real credentials before generating configs."
    }
    if ([string]::IsNullOrWhiteSpace($adminPass) -or $adminPass.StartsWith("__CHANGE_ME_")) {
        throw "MT2_ADMINPAGE_PASSWORD is not configured or still uses a placeholder in $EnvFile."
    }
    if ([string]::IsNullOrWhiteSpace($serverKey) -or $serverKey.StartsWith("__CHANGE_ME_")) {
        throw "MT2_SERVER_KEY is not configured or still uses a placeholder in $EnvFile."
    }

    $confTxt = @"
WELCOME_MSG = "Mysql odpalone"
SQL_ACCOUNT = "$dbHost account $dbUser $dbPass $dbPort"
SQL_HOTBACKUP = "$dbHost player $dbUser $dbPass $dbPort"
SQL_PLAYER = "$dbHost player $dbUser $dbPass $dbPort"
SQL_COMMON = "$dbHost common $dbUser $dbPass $dbPort"
SQL_LOG = "$dbHost log $logDbUser $logDbPass $dbPort"
SQL_WORLD = "$dbHost world $dbUser $dbPass $dbPort"
TABLE_POSTFIX = ""
BIND_PORT = $dbListenPort
DB_SLEEP_MSEC = 10
CLIENT_HEART_FPS = 10
HASH_PLAYER_LIFE_SEC = 600
BACKUP_LIMIT_SEC = 3600
PLAYER_ID_START = 100
PLAYER_DELETE_LEVEL_LIMIT = 75
PLAYER_DELETE_CHECK_SIMPLE = 1
ITEM_ID_RANGE = 2000000000 2100000000
PROTO_FROM_DB = 1
MIRROR2DB = 1
DISABLE_HOTBACKUP = 1
"@
    $dbConfPath = Join-Path $TargetDir "db\conf.txt"
    [System.IO.File]::WriteAllText($dbConfPath, $confTxt, [System.Text.Encoding]::ASCII)
    Protect-SensitiveFile $dbConfPath

    $authCfg = @"
HOSTNAME: auth
CHANNEL: 1
PORT: $authPort
P2P_PORT: $authP2pPort
DB_PORT: $dbListenPort
DB_ADDR: $dbAddr
PASSES_PER_SEC: 25
SAVE_EVENT_SECOND_CYCLE: 180
PING_EVENT_SECOND_CYCLE: 180
TABLE_POSTFIX: 
ITEM_ID_RANGE: 000000001 000000002
AUTH_SERVER: master
ACCOUNT_SQL: $dbHost $dbUser $dbPass account $dbPort
COMMON_SQL: $dbHost $dbUser $dbPass common $dbPort
LOG_SQL: $dbHost $logDbUser $logDbPass log $dbPort
SQL_HOTBACKUP: $dbHost $dbUser $dbPass player $dbPort
PLAYER_SQL: $dbHost $dbUser $dbPass player $dbPort
SQL_WORLD: $dbHost $dbUser $dbPass world $dbPort
LOCALE_SERVICE: $localeSvc
BIND_IP: $bindIp
"@
    $authCfgPath = Join-Path $TargetDir "auth\CONFIG"
    [System.IO.File]::WriteAllText($authCfgPath, $authCfg, [System.Text.Encoding]::ASCII)
    Protect-SensitiveFile $authCfgPath

    $channels = @(
        @{ Name="ch11"; Host="channel1";  Ch="1";  Port="28000"; P2P="29000"; Maps="1 21 41" },
        @{ Name="ch12"; Host="channel1";  Ch="1";  Port="28001"; P2P="29001"; Maps="3 23 43 4 24 44 63 65 67 68 64 61 62 70 104" },
        @{ Name="ch13"; Host="channel1";  Ch="1";  Port="28002"; P2P="29002"; Maps="66 69 79 112 181 182 183 5 25 45 108 109" },
        @{ Name="ch99"; Host="channel99"; Ch="99"; Port="13099"; P2P="14099"; Maps="110 111 81 113 200 90" },
        @{ Name="ch00"; Host="channel00"; Ch="99"; Port="13088"; P2P="14088"; Maps="250" },
        @{ Name="ch01"; Host="channel01"; Ch="99"; Port="13098"; P2P="14098"; Maps="71" }
    )
    foreach ($c in $channels) {
        $chCfg = @"
HOSTNAME: $($c.Host)
CHANNEL: $($c.Ch)
PORT: $($c.Port)
P2P_PORT: $($c.P2P)
DB_PORT: $dbListenPort
DB_ADDR: $dbAddr
MAP_ALLOW: $($c.Maps)
TABLE_POSTFIX: 
PASSES_PER_SEC: 25
SAVE_EVENT_SECOND_CYCLE: 180
PING_EVENT_SECOND_CYCLE: 180
ACCOUNT_SQL: $dbHost $dbUser $dbPass account $dbPort
COMMON_SQL: $dbHost $dbUser $dbPass common $dbPort
LOG_SQL: $dbHost $logDbUser $logDbPass log $dbPort
SQL_HOTBACKUP: $dbHost $dbUser $dbPass player $dbPort
PLAYER_SQL: $dbHost $dbUser $dbPass player $dbPort
SQL_WORLD: $dbHost $dbUser $dbPass world $dbPort
SERVER_KEY: $serverKey
MALL_URL: localhost
ADMINPAGE_IP: $adminIp
ADMINPAGE_PASSWORD: $adminPass
VIEW_RANGE: 8000
CHECK_MULTIHACK: 0
MAX_LEVEL: 120
SPEEDHACK_LIMIT_COUNT: 300
SPEEDHACK_LIMIT_BONUS: 80
PK_PROTECT_LEVEL: 15
TEST_SERVER: 0
XTRAP_ENABLE: 0
GLOBAL_SHOUT: 1
SHOP_PRICE_3X_TAX: 0
BIND_IP: $bindIp
"@
        $chCfgPath = Join-Path $TargetDir "chs\$($c.Name)\CONFIG"
        [System.IO.File]::WriteAllText($chCfgPath, $chCfg, [System.Text.Encoding]::ASCII)
        Protect-SensitiveFile $chCfgPath
    }
    Protect-SensitiveFile $EnvFile
}

if ($InitSql) {
    Write-Host "=== [4/4] Non-Destructive 5-Database Schema Initialization ==="
    $dbBinDir = Get-ConfigValue $cfg "MT2_DB_BIN_DIR" ""
    $mysqlCli = $null
    if (![string]::IsNullOrWhiteSpace($dbBinDir)) {
        foreach ($cand in @("mariadb.exe", "mysql.exe")) {
            $p = Join-Path $dbBinDir $cand
            if (Test-Path $p) { $mysqlCli = $p; break }
        }
    }
    if (!$mysqlCli) {
        $cmd = Get-Command mariadb.exe, mysql.exe -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($cmd) { $mysqlCli = $cmd.Source }
    }
    if (!$mysqlCli) {
        throw "Neither mariadb.exe nor mysql.exe found in MT2_DB_BIN_DIR or PATH."
    }

    $tmpCnf = Join-Path $TargetDir ".sql_init_client.cnf"
    $prevEap = $ErrorActionPreference
    try {
        $cnfContent = "[client]`r`nhost=$dbHost`r`nport=$dbPort`r`nuser=$initDbUser`r`npassword=`"$initDbPass`"`r`n"
        [System.IO.File]::WriteAllText($tmpCnf, $cnfContent, [System.Text.Encoding]::ASCII)
        Protect-SensitiveFile $tmpCnf
        $ErrorActionPreference = "Continue"
        $strictMode = "STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION"
        foreach ($dbName in @("account", "common", "log", "player", "world")) {
            $tblOut = & $mysqlCli "--defaults-extra-file=$tmpCnf" -N -s -e "SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA='$dbName';" 2>$null
            if ($LASTEXITCODE -ne 0) {
                throw "Failed querying table count for database '$dbName'"
            }
            $tblCount = ($tblOut | Out-String).Trim()
            if ($tblCount -eq "0") {
                $sqlPath = Join-Path $repoRoot "sql\$dbName.sql"
                Write-Host "  -> Loading $sqlPath into empty database '$dbName'..."
                Get-Content -Raw $sqlPath | & $mysqlCli "--defaults-extra-file=$tmpCnf" $dbName --init-command="SET SESSION sql_mode='$strictMode';"
                if ($LASTEXITCODE -ne 0) {
                    throw "Failed loading $sqlPath into '$dbName'"
                }
            } else {
                Write-Host "  -> Database '$dbName' already contains $tblCount tables; skipping destructive reload to preserve data."
            }
        }
    } finally {
        $ErrorActionPreference = $prevEap
        if (Test-Path $tmpCnf) {
            Remove-Item -Force $tmpCnf -ErrorAction SilentlyContinue
        }
    }
}

Write-Host "=== [OK] Windows x64 Serverfiles Installed at $TargetDir ==="
