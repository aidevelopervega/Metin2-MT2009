param(
    [string]$ServerRoot = ""
)
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

$services = @(
    @{ Name="mysql"; Port=$dbPort;       Dir=(Join-Path $SFROOT "mysql_data") },
    @{ Name="db";    Port=$dbListenPort; Dir=(Join-Path $SFROOT "db") },
    @{ Name="auth";  Port=$authPort;     Dir=(Join-Path $SFROOT "auth") },
    @{ Name="ch11";  Port=28000;         Dir=(Join-Path $SFROOT "chs\ch11") },
    @{ Name="ch12";  Port=28001;         Dir=(Join-Path $SFROOT "chs\ch12") },
    @{ Name="ch13";  Port=28002;         Dir=(Join-Path $SFROOT "chs\ch13") },
    @{ Name="ch99";  Port=13099;         Dir=(Join-Path $SFROOT "chs\ch99") },
    @{ Name="ch00";  Port=13088;         Dir=(Join-Path $SFROOT "chs\ch00") },
    @{ Name="ch01";  Port=13098;         Dir=(Join-Path $SFROOT "chs\ch01") }
)
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

function Get-ServicePid([string]$svcDir, [int]$port) {
    try {
        $conn = Get-NetTCPConnection -LocalPort $port -State Listen -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($conn -and $conn.OwningProcess) { return "$($conn.OwningProcess)" }
    } catch {}
    foreach ($pf in @("pid", "pid.win", "mariadbd.pid")) {
        $pPath = Join-Path $svcDir $pf
        if (Test-Path $pPath) {
            $rawPid = (Get-Content $pPath -ErrorAction SilentlyContinue | Out-String).Trim()
            if (![string]::IsNullOrWhiteSpace($rawPid)) { return $rawPid }
        }
    }
    return "active"
}

foreach ($s in $services) {
    $listening = Test-PortListening $s.Port
    $status = if ($listening) { "LISTENING (PID $(Get-ServicePid $s.Dir $s.Port))" } else { "STOPPED" }
    $syserrPath = Join-Path $s.Dir "syserr.txt"
    if (-not (Test-Path $syserrPath)) { $syserrPath = Join-Path $s.Dir "syserr" }
    $syserrLines = if (Test-Path $syserrPath) { @(Get-Content $syserrPath).Count } else { 0 }
    Write-Host ("{0,-8} Port {1,-6} : {2,-26} | syserr lines: {3}" -f $s.Name, $s.Port, $status, $syserrLines)
}
