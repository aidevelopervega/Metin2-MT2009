param(
    [switch]$StopMySQL,
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
$logMaxBytes = [int64](Get-ConfigValue $cfg "MT2_LOG_MAX_BYTES" "10485760")
$logMaxArchives = [int](Get-ConfigValue $cfg "MT2_LOG_MAX_ARCHIVES" "10")

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

function Get-ServicePids([string]$svcDir) {
    $pids = @()
    foreach ($pf in @("pid", "pid.win")) {
        $pPath = Join-Path $svcDir $pf
        if (Test-Path $pPath) {
            $rawPid = (Get-Content $pPath -ErrorAction SilentlyContinue | Out-String).Trim()
            $pidNum = 0
            if ([int]::TryParse($rawPid, [ref]$pidNum) -and $pidNum -gt 0) {
                $pids += $pidNum
            }
        }
    }
    return $pids
}

function Test-PidsAlive([int[]]$pids) {
    foreach ($procId in $pids) {
        if ($procId -gt 0) {
            $p = Get-Process -Id $procId -ErrorAction SilentlyContinue
            if ($p -and ($p.ProcessName -match "^(db_r41023|game_r41023|cmd)")) {
                return $true
            }
        }
    }
    return $false
}

function Stop-ServiceDirectory([string]$svcName, [string]$svcDir, [int]$port, [int]$timeoutIter = 40, [int[]]$trackedPids = @(), [bool]$alreadySignaled = $false) {
    if (!(Test-Path $svcDir)) { return }
    $pidFile = Join-Path $svcDir "pid"
    if ($trackedPids.Count -eq 0) {
        $trackedPids = Get-ServicePids $svcDir
    }
    $hasPid = $alreadySignaled -or (Test-Path $pidFile) -or (Test-PortListening $port) -or (Test-PidsAlive $trackedPids)
    if ($hasPid) {
        if (-not $alreadySignaled) {
            Write-Host "  -> Signaling graceful stop for $svcName (port $port)..." -ForegroundColor Yellow
            Set-Content -Path (Join-Path $svcDir "stop_signal") -Value "1" -Encoding ASCII
        }
        for ($i = 0; $i -lt $timeoutIter; $i++) {
            Start-Sleep -Milliseconds 250
            if (-not (Test-PortListening $port) -and !(Test-Path $pidFile) -and -not (Test-PidsAlive $trackedPids)) { break }
        }
        if ((Test-PortListening $port) -or (Test-PidsAlive $trackedPids)) {
            foreach ($procId in $trackedPids) {
                Stop-Process -Id $procId -Force -ErrorAction SilentlyContinue
            }
            Start-Sleep -Milliseconds 200
        }
    }
    foreach ($f in @("pid", "pid.win", "stop_signal")) {
        $p = Join-Path $svcDir $f
        if (Test-Path $p) { Remove-Item -Force $p -ErrorAction SilentlyContinue }
    }
}

function Rotate-ServiceLogs([string]$svcDir, [bool]$forceRotate) {
    if (!(Test-Path $svcDir)) { return }
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

Write-Host "=== [1/3] Stopping AUTH Server ===" -ForegroundColor Cyan
Stop-ServiceDirectory "auth" (Join-Path $SFROOT "auth") $authPort 40
Rotate-ServiceLogs (Join-Path $SFROOT "auth") $RotateLogs.IsPresent

Write-Host "=== [2/3] Stopping Game Channels & Flushing Player/Item/Quest State ===" -ForegroundColor Cyan
$chDefs = @(
    @{ Name="ch11"; Port=28000 },
    @{ Name="ch12"; Port=28001 },
    @{ Name="ch13"; Port=28002 },
    @{ Name="ch99"; Port=13099 },
    @{ Name="ch00"; Port=13088 },
    @{ Name="ch01"; Port=13098 }
)
$chPidsMap = @{}
$chSignaledMap = @{}
foreach ($cd in $chDefs) {
    $cDir = Join-Path $SFROOT "chs\$($cd.Name)"
    if (Test-Path $cDir) {
        $pids = Get-ServicePids $cDir
        $chPidsMap[$cd.Name] = $pids
        if ((Test-Path (Join-Path $cDir "pid")) -or (Test-PortListening $cd.Port) -or (Test-PidsAlive $pids)) {
            Write-Host "  -> Signaling graceful stop for $($cd.Name) (port $($cd.Port))..." -ForegroundColor Yellow
            Set-Content -Path (Join-Path $cDir "stop_signal") -Value "1" -Encoding ASCII
            $chSignaledMap[$cd.Name] = $true
        } else {
            $chSignaledMap[$cd.Name] = $false
        }
    }
}
foreach ($cd in $chDefs) {
    $cDir = Join-Path $SFROOT "chs\$($cd.Name)"
    $pids = if ($chPidsMap.ContainsKey($cd.Name)) { $chPidsMap[$cd.Name] } else { @() }
    $sig = if ($chSignaledMap.ContainsKey($cd.Name)) { $chSignaledMap[$cd.Name] } else { $false }
    Stop-ServiceDirectory $cd.Name $cDir $cd.Port 60 $pids $sig
    Rotate-ServiceLogs $cDir $RotateLogs.IsPresent
}

Write-Host "=== [3/3] Stopping DB Cache Server & Draining SQL Write Queue ===" -ForegroundColor Cyan
Stop-ServiceDirectory "db" (Join-Path $SFROOT "db") $dbListenPort 60
Rotate-ServiceLogs (Join-Path $SFROOT "db") $RotateLogs.IsPresent

if ($StopMySQL) {
    $pidPath = Join-Path $SFROOT "mysql_data\mariadbd.pid"
    if (Test-Path $pidPath) {
        $mPid = [int](Get-Content $pidPath).Trim()
        Stop-Process -Id $mPid -ErrorAction SilentlyContinue
        Remove-Item -Force $pidPath -ErrorAction SilentlyContinue
    }
}
Write-Host "All MT2009 Windows x64 server processes stopped cleanly." -ForegroundColor Green
