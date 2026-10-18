param(
    [int]$Channels = 1,
    [switch]$UseLua50,
    [switch]$RotateLogs,
    [string]$ServerRoot = ""
)
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if (![string]::IsNullOrWhiteSpace($ServerRoot)) {
    $SFROOT = (Resolve-Path $ServerRoot).Path
} elseif (Test-Path (Join-Path $scriptDir "share\bin")) {
    $SFROOT = $scriptDir
} elseif (Test-Path (Join-Path $scriptDir "..\..\files\share\bin")) {
    $SFROOT = (Resolve-Path (Join-Path $scriptDir "..\..\files")).Path
} else {
    $SFROOT = $scriptDir
}

function Load-EnvMap([string]$path) {
    $map = @{}
    if ([string]::IsNullOrWhiteSpace($path) -or !(Test-Path $path)) { return $map }
    foreach ($line in Get-Content $path) {
        $trimmed = $line.Trim()
        if ($trimmed.Length -eq 0 -or $trimmed.StartsWith("#")) { continue }
        $idx = $trimmed.IndexOf("=")
        if ($idx -gt 0) {
            $map[$trimmed.Substring(0, $idx).Trim()] = $trimmed.Substring($idx + 1).Trim()
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

$cfg = Load-EnvMap (Join-Path $SFROOT "serverfiles.env")
$dbPort = [int](Get-ConfigValue $cfg "MT2_DB_PORT" "3306")
$dbListenPort = [int](Get-ConfigValue $cfg "MT2_DB_LISTEN_PORT" "18000")
$authPort = [int](Get-ConfigValue $cfg "MT2_AUTH_PORT" "17105")
$dbBinDir = Get-ConfigValue $cfg "MT2_DB_BIN_DIR" ""
$dataDir = Get-ConfigValue $cfg "MT2_MYSQL_DATADIR" (Join-Path $SFROOT "mysql_data")
$logMaxBytes = [int64](Get-ConfigValue $cfg "MT2_LOG_MAX_BYTES" "10485760")
$logMaxArchives = [int](Get-ConfigValue $cfg "MT2_LOG_MAX_ARCHIVES" "10")

$gameBin = if ($UseLua50) { Join-Path $SFROOT "share\bin\game_r41023_lua50.exe" } else { Join-Path $SFROOT "share\bin\game_r41023.exe" }
$dbBin = Join-Path $SFROOT "share\bin\db_r41023.exe"

function Start-DetachedService([string]$CommandLine, [string]$WorkingDir, [string]$ExePath = "", [string[]]$ArgumentList = @(), [string]$StdOut = "", [string]$StdErr = "") {
    try {
        $startup = New-CimInstance -ClassName Win32_ProcessStartup -Property @{ ShowWindow = [uint16]0 } -ClientOnly -ErrorAction Stop
        $res = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{
            CommandLine = $CommandLine
            CurrentDirectory = $WorkingDir
            ProcessStartupInformation = $startup
        } -ErrorAction Stop
        if ($res.ReturnValue -eq 0) {
            return [int]$res.ProcessId
        }
    } catch {
        # Fallback to Start-Process when WMI/CIM is restricted
    }
    if (![string]::IsNullOrWhiteSpace($ExePath)) {
        $spArgs = @{
            FilePath = $ExePath
            WorkingDirectory = $WorkingDir
            WindowStyle = "Hidden"
            PassThru = $true
        }
        if ($ArgumentList -and $ArgumentList.Count -gt 0) { $spArgs["ArgumentList"] = $ArgumentList }
        if (![string]::IsNullOrWhiteSpace($StdOut)) { $spArgs["RedirectStandardOutput"] = $StdOut }
        if (![string]::IsNullOrWhiteSpace($StdErr)) { $spArgs["RedirectStandardError"] = $StdErr }
        $p = Start-Process @spArgs
        return [int]$p.Id
    }
    $p2 = Start-Process -FilePath "cmd.exe" -ArgumentList "/c `"$CommandLine`"" -WorkingDirectory $WorkingDir -WindowStyle Hidden -PassThru
    return [int]$p2.Id
}

function Test-PortListening([int]$port) {
    try {
        $conn = Get-NetTCPConnection -LocalPort $port -State Listen -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($conn) { return $true }
    } catch {}
    try {
        $listeners = [System.Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveTcpListeners()
        foreach ($ep in $listeners) {
            if ($ep.Port -eq $port) { return $true }
        }
    } catch {}
    return $false
}

function Clear-StaleState([string]$svcName, [string]$svcDir, [int]$port) {
    foreach ($pf in @("pid", "pid.win")) {
        $pPath = Join-Path $svcDir $pf
        if (Test-Path $pPath) {
            $rawPid = (Get-Content $pPath -ErrorAction SilentlyContinue | Out-String).Trim()
            $pidNum = 0
            if ([int]::TryParse($rawPid, [ref]$pidNum) -and $pidNum -gt 0) {
                $proc = Get-Process -Id $pidNum -ErrorAction SilentlyContinue
                if ($proc -and ($proc.ProcessName -match "^(db_r41023|game_r41023)")) {
                    throw "Service '$svcName' is already running (PID $pidNum). Run stop.ps1 before starting."
                }
            }
            Remove-Item -Force $pPath -ErrorAction SilentlyContinue
        }
    }
    $stopSig = Join-Path $svcDir "stop_signal"
    if (Test-Path $stopSig) {
        Remove-Item -Force $stopSig -ErrorAction SilentlyContinue
    }
    if (Test-PortListening $port) {
        throw "Port $port required by '$svcName' is already in use."
    }
}

function Rotate-ServiceLogs([string]$svcDir, [bool]$forceRotate) {
    $archiveDir = Join-Path $svcDir "log\archive"
    New-Item -ItemType Directory -Force -Path $archiveDir | Out-Null
    $stamp = (Get-Date).ToString("yyyyMMdd_HHmmss")
    $logNames = @("syserr", "syserr.txt", "syslog", "syslog.txt", "PTS", "PTS.txt", "stdout.log", "stderr.log")
    foreach ($ln in $logNames) {
        $lp = Join-Path $svcDir $ln
        if (Test-Path $lp) {
            $item = Get-Item $lp -ErrorAction SilentlyContinue
            if ($item -and $item.Length -gt 0 -and ($forceRotate -or $item.Length -ge $logMaxBytes)) {
                $dest = Join-Path $archiveDir "${ln}.${stamp}.log"
                Move-Item -Force $lp $dest -ErrorAction SilentlyContinue
            }
        }
        $archived = @(Get-ChildItem -Path $archiveDir -Filter "${ln}.*.log" -File -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending)
        if ($archived.Count -gt $logMaxArchives) {
            $archived | Select-Object -Skip $logMaxArchives | Remove-Item -Force -ErrorAction SilentlyContinue
        }
    }
}

$chDefs = @(
    @{ Name="ch11"; Port=28000 },
    @{ Name="ch12"; Port=28001 },
    @{ Name="ch13"; Port=28002 },
    @{ Name="ch99"; Port=13099 },
    @{ Name="ch00"; Port=13088 },
    @{ Name="ch01"; Port=13098 }
)

Clear-StaleState "db" (Join-Path $SFROOT "db") $dbListenPort
Rotate-ServiceLogs (Join-Path $SFROOT "db") $RotateLogs.IsPresent
Clear-StaleState "auth" (Join-Path $SFROOT "auth") $authPort
Rotate-ServiceLogs (Join-Path $SFROOT "auth") $RotateLogs.IsPresent
foreach ($cd in $chDefs) {
    $cDir = Join-Path $SFROOT "chs\$($cd.Name)"
    if (Test-Path (Join-Path $cDir "CONFIG")) {
        Clear-StaleState $cd.Name $cDir $cd.Port
        Rotate-ServiceLogs $cDir $RotateLogs.IsPresent
    }
}

Write-Host "=== [1/4] Checking / Starting SQL Server (127.0.0.1:$dbPort) ===" -ForegroundColor Cyan
if (-not (Test-PortListening $dbPort)) {
    $mariaDaemon = $null
    if (![string]::IsNullOrWhiteSpace($dbBinDir)) {
        foreach ($cand in @("mariadbd.exe", "mysqld.exe")) {
            $p = Join-Path $dbBinDir $cand
            if (Test-Path $p) { $mariaDaemon = $p; break }
        }
    }
    if (!$mariaDaemon) {
        $cmd = Get-Command mariadbd.exe, mysqld.exe -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($cmd) { $mariaDaemon = $cmd.Source }
    }
    if (!$mariaDaemon -or !(Test-Path $dataDir)) {
        throw "SQL server is not listening on port $dbPort, and no local daemon/datadir (MT2_DB_BIN_DIR / MT2_MYSQL_DATADIR) is configured."
    }
    $mArgs = @("--datadir=$dataDir", "--port=$dbPort", "--bind-address=127.0.0.1", "--sql-mode=STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION", "--max-allowed-packet=64M")
    $mCmd = "`"$mariaDaemon`" --datadir=`"$dataDir`" --port=$dbPort --bind-address=127.0.0.1 --sql-mode=STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION --max-allowed-packet=64M"
    $mPid = Start-DetachedService -CommandLine $mCmd -WorkingDir $SFROOT -ExePath $mariaDaemon -ArgumentList $mArgs
    Set-Content -Path (Join-Path $dataDir "mariadbd.pid") -Value $mPid -Encoding ASCII
    for ($i = 0; $i -lt 40; $i++) {
        Start-Sleep -Milliseconds 500
        if (Test-PortListening $dbPort) { break }
    }
    if (-not (Test-PortListening $dbPort)) {
        throw "SQL server failed to start on port $dbPort"
    }
}
Write-Host "  -> SQL Server ONLINE (port $dbPort)" -ForegroundColor Green

Write-Host "=== [2/4] Starting DB Cache Server (127.0.0.1:$dbListenPort) ===" -ForegroundColor Cyan
$dbDir = Join-Path $SFROOT "db"
$dbCmd = "cmd.exe /c `"`"$dbBin`" > stdout.log 2> stderr.log`""
$dbPid = Start-DetachedService -CommandLine $dbCmd -WorkingDir $dbDir -ExePath $dbBin -StdOut (Join-Path $dbDir "stdout.log") -StdErr (Join-Path $dbDir "stderr.log")
Set-Content -Path (Join-Path $SFROOT "db\pid.win") -Value $dbPid -Encoding ASCII
for ($i = 0; $i -lt 40; $i++) {
    Start-Sleep -Milliseconds 500
    if (Test-PortListening $dbListenPort) { break }
}
if (-not (Test-PortListening $dbListenPort)) {
    throw "DB Cache Server failed to listen on port $dbListenPort"
}
Write-Host "  -> DB Cache Server listening on port $dbListenPort" -ForegroundColor Green

Write-Host "=== [3/4] Starting Game Channels ===" -ForegroundColor Cyan
foreach ($cd in $chDefs) {
    $ch = $cd.Name
    $chDir = Join-Path $SFROOT "chs\$ch"
    if (Test-Path (Join-Path $chDir "CONFIG")) {
        $chCmd = "cmd.exe /c `"`"$gameBin`" > stdout.log 2> stderr.log`""
        $procId = Start-DetachedService -CommandLine $chCmd -WorkingDir $chDir -ExePath $gameBin -StdOut (Join-Path $chDir "stdout.log") -StdErr (Join-Path $chDir "stderr.log")
        Set-Content -Path (Join-Path $chDir "pid.win") -Value $procId -Encoding ASCII
        Write-Host "  -> $ch launched (launcher PID $procId)" -ForegroundColor Green
        Start-Sleep -Milliseconds 300
    }
}

Write-Host "=== [4/4] Starting AUTH Server (127.0.0.1:$authPort) ===" -ForegroundColor Cyan
$authDir = Join-Path $SFROOT "auth"
$authCmd = "cmd.exe /c `"`"$gameBin`" > stdout.log 2> stderr.log`""
$authPid = Start-DetachedService -CommandLine $authCmd -WorkingDir $authDir -ExePath $gameBin -StdOut (Join-Path $authDir "stdout.log") -StdErr (Join-Path $authDir "stderr.log")
Set-Content -Path (Join-Path $SFROOT "auth\pid.win") -Value $authPid -Encoding ASCII
Write-Host "  -> auth launched (launcher PID $authPid)" -ForegroundColor Green

$expectedPorts = @($authPort) + ($chDefs | ForEach-Object { $_.Port })
foreach ($ep in $expectedPorts) {
    for ($i = 0; $i -lt 60; $i++) {
        if (Test-PortListening $ep) { break }
        Start-Sleep -Milliseconds 500
    }
    if (-not (Test-PortListening $ep)) {
        throw "Timeout waiting for port $ep to enter LISTENING state"
    }
}

function Wait-ChannelBoot([string]$svcName, [string]$svcDir) {
    for ($i = 0; $i -lt 60; $i++) {
        foreach ($ln in @("syslog.txt", "syslog")) {
            $lp = Join-Path $svcDir $ln
            if (Test-Path $lp) {
                $bootHits = @(Select-String -Path $lp -Pattern "Start of pid:|LoadLocaleFile: DragonSoulTable:|XXX Boot Call NPC" -ErrorAction SilentlyContinue)
                if ($bootHits.Count -gt 0) {
                    $lastLine = $bootHits[$bootHits.Count - 1].Line
                    if ($lastLine -match "LoadLocaleFile: DragonSoulTable:|XXX Boot Call NPC") {
                        return
                    }
                }
            }
        }
        Start-Sleep -Milliseconds 500
    }
    throw "Timeout waiting for channel '$svcName' to complete CInputDB::Boot (DragonSoulTable)"
}

foreach ($cd in $chDefs) {
    $cDir = Join-Path $SFROOT "chs\$($cd.Name)"
    if (Test-Path (Join-Path $cDir "CONFIG")) {
        Wait-ChannelBoot $cd.Name $cDir
    }
}
Write-Host "=== All MT2009 Windows x64 services are ONLINE ===" -ForegroundColor Green
