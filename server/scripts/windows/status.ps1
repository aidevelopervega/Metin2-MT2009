$SFROOT = Split-Path -Parent $MyInvocation.MyCommand.Path
$services = @(
    @{ Name="mysql"; Port=3306; Dir="$SFROOT\mysql_data" },
    @{ Name="db";    Port=18000; Dir="$SFROOT\db" },
    @{ Name="auth";  Port=17105; Dir="$SFROOT\auth" },
    @{ Name="ch11";  Port=28000; Dir="$SFROOT\chs\ch11" },
    @{ Name="ch12";  Port=28001; Dir="$SFROOT\chs\ch12" },
    @{ Name="ch13";  Port=28002; Dir="$SFROOT\chs\ch13" },
    @{ Name="ch99";  Port=13099; Dir="$SFROOT\chs\ch99" },
    @{ Name="ch00";  Port=13088; Dir="$SFROOT\chs\ch00" },
    @{ Name="ch01";  Port=13098; Dir="$SFROOT\chs\ch01" }
)
foreach ($s in $services) {
    $conn = Get-NetTCPConnection -LocalPort $s.Port -State Listen -ErrorAction SilentlyContinue | Select-Object -First 1
    $status = if ($conn) { "LISTENING (PID $($conn.OwningProcess))" } else { "STOPPED" }
    $syserrPath = Join-Path $s.Dir "syserr.txt"
    if (-not (Test-Path $syserrPath)) { $syserrPath = Join-Path $s.Dir "syserr" }
    $syserrLines = if (Test-Path $syserrPath) { @(Get-Content $syserrPath).Count } else { 0 }
    Write-Host ("{0,-8} Port {1,-6} : {2,-26} | syserr lines: {3}" -f $s.Name, $s.Port, $status, $syserrLines)
}
