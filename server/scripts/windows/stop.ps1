param(
    [switch]$StopMySQL
)
$SFROOT = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent (Split-Path -Parent $SFROOT)
$mariaBin = Join-Path $repoRoot "scratch\isolated_db\mariadb-12.3.3-winx64\bin"

$gameDirs = @("$SFROOT\auth")
Get-ChildItem -Path "$SFROOT\chs" -Directory -ErrorAction SilentlyContinue | ForEach-Object { $gameDirs += $_.FullName }

$gameProcs = Get-Process -Name "game_r41023","game_r41023_lua50" -ErrorAction SilentlyContinue
if ($gameProcs) {
    Write-Host "Sending graceful shutdown signal to game/auth processes ($($gameProcs.Count) running)..." -ForegroundColor Yellow
    foreach ($d in $gameDirs) {
        if (Test-Path $d) {
            Set-Content -Path "$d\stop_signal" -Value "1" -Encoding ASCII
        }
    }
    for ($i = 0; $i -lt 20; $i++) {
        Start-Sleep -Milliseconds 300
        $remaining = Get-Process -Name "game_r41023","game_r41023_lua50" -ErrorAction SilentlyContinue
        if (-not $remaining) { break }
    }
    $remaining = Get-Process -Name "game_r41023","game_r41023_lua50" -ErrorAction SilentlyContinue
    if ($remaining) {
        $remaining | Stop-Process -Force -ErrorAction SilentlyContinue
    }
}

$dbProcs = Get-Process -Name "db_r41023" -ErrorAction SilentlyContinue
if ($dbProcs) {
    Write-Host "Sending graceful shutdown signal to DB Cache Server..." -ForegroundColor Yellow
    Set-Content -Path "$SFROOT\db\stop_signal" -Value "1" -Encoding ASCII
    for ($i = 0; $i -lt 20; $i++) {
        Start-Sleep -Milliseconds 300
        $remainingDb = Get-Process -Name "db_r41023" -ErrorAction SilentlyContinue
        if (-not $remainingDb) { break }
    }
    $remainingDb = Get-Process -Name "db_r41023" -ErrorAction SilentlyContinue
    if ($remainingDb) {
        $remainingDb | Stop-Process -Force -ErrorAction SilentlyContinue
    }
}

Get-ChildItem -Path $SFROOT -Filter "pid*" -Recurse -File -Force -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
Get-ChildItem -Path $SFROOT -Filter "stop_signal" -Recurse -File -Force -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue

if ($StopMySQL) {
    Write-Host "Stopping MariaDB..." -ForegroundColor Yellow
    & "$mariaBin\mariadb-admin.exe" -h 127.0.0.1 -P 3306 -u root shutdown 2>$null
}
Write-Host "All MT2009 server processes stopped." -ForegroundColor Green
