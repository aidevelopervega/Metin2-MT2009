param(
    [int]$Channels = 1,
    [switch]$UseLua50
)
$ErrorActionPreference = "Stop"
$SFROOT = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent (Split-Path -Parent $SFROOT)
$mariaBin = Join-Path $repoRoot "scratch\isolated_db\mariadb-12.3.3-winx64\bin"
$dataDir = "$SFROOT\mysql_data"
$gameBin = if ($UseLua50) { "$SFROOT\share\bin\game_r41023_lua50.exe" } else { "$SFROOT\share\bin\game_r41023.exe" }
$dbBin = "$SFROOT\share\bin\db_r41023.exe"

function Start-DetachedService([string]$CommandLine, [string]$WorkingDir) {
    $startup = New-CimInstance -ClassName Win32_ProcessStartup -Property @{ ShowWindow = [uint16]0 } -ClientOnly
    $res = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{
        CommandLine = $CommandLine
        CurrentDirectory = $WorkingDir
        ProcessStartupInformation = $startup
    }
    if ($res.ReturnValue -ne 0) {
        throw "Failed to create process: $CommandLine (code $($res.ReturnValue))"
    }
    return [int]$res.ProcessId
}

Write-Host "=== [1/4] Checking / Starting MariaDB (127.0.0.1:3306) ===" -ForegroundColor Cyan
$mariaListening = Get-NetTCPConnection -LocalPort 3306 -State Listen -ErrorAction SilentlyContinue
if (-not $mariaListening) {
    $mCmd = "`"$mariaBin\mariadbd.exe`" --datadir=`"$dataDir`" --port=3306 --bind-address=127.0.0.1 --sql-mode=STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION --max-allowed-packet=64M"
    $mPid = Start-DetachedService -CommandLine $mCmd -WorkingDir $SFROOT
    Set-Content -Path "$SFROOT\mysql_data\mariadbd.pid" -Value $mPid -Encoding ASCII
    for ($i = 0; $i -lt 30; $i++) {
        Start-Sleep -Milliseconds 500
        & "$mariaBin\mariadb-admin.exe" -h 127.0.0.1 -P 3306 ping 2>$null | Out-Null
        if ($LASTEXITCODE -eq 0) { break }
    }
}
Write-Host "  -> MariaDB ONLINE (port 3306)" -ForegroundColor Green

Write-Host "=== [2/4] Starting DB Cache Server (127.0.0.1:18000) ===" -ForegroundColor Cyan
$dbCmd = "cmd.exe /c `"`"$dbBin`" > stdout.log 2> stderr.log`""
$dbPid = Start-DetachedService -CommandLine $dbCmd -WorkingDir "$SFROOT\db"
Set-Content -Path "$SFROOT\db\pid.win" -Value $dbPid -Encoding ASCII
for ($i = 0; $i -lt 40; $i++) {
    Start-Sleep -Milliseconds 500
    if (Get-NetTCPConnection -LocalPort 18000 -State Listen -ErrorAction SilentlyContinue) { break }
}
Write-Host "  -> DB Cache Server listening on port 18000" -ForegroundColor Green

Write-Host "=== [3/4] Starting Game Channels ===" -ForegroundColor Cyan
$chList = @()
for ($c = 1; $c -le $Channels; $c++) {
    $chList += "ch${c}1", "ch${c}2", "ch${c}3"
}
$chList += "ch99", "ch00", "ch01"

foreach ($ch in $chList) {
    $chDir = "$SFROOT\chs\$ch"
    if (Test-Path "$chDir\CONFIG") {
        $chCmd = "cmd.exe /c `"`"$gameBin`" > stdout.log 2> stderr.log`""
        $procId = Start-DetachedService -CommandLine $chCmd -WorkingDir $chDir
        Set-Content -Path "$chDir\pid.win" -Value $procId -Encoding ASCII
        Write-Host "  -> $ch launched (launcher PID $procId)" -ForegroundColor Green
        Start-Sleep -Milliseconds 400
    }
}

Write-Host "=== [4/4] Starting AUTH Server (127.0.0.1:17105) ===" -ForegroundColor Cyan
$authCmd = "cmd.exe /c `"`"$gameBin`" > stdout.log 2> stderr.log`""
$authPid = Start-DetachedService -CommandLine $authCmd -WorkingDir "$SFROOT\auth"
Set-Content -Path "$SFROOT\auth\pid.win" -Value $authPid -Encoding ASCII
Write-Host "  -> auth launched (launcher PID $authPid)" -ForegroundColor Green
