param(
    [ValidateSet("Backup", "Restore")]
    [string]$Action = "Backup",
    [string]$BackupDir = "",
    [string]$EnvFile = "",
    [string[]]$Databases = @("account", "common", "log", "player", "world"),
    [string]$RestoreArchive = "",
    [string]$TargetDatabase = ""
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if (Test-Path (Join-Path $scriptDir "share\bin")) {
    $SFROOT = $scriptDir
} elseif (Test-Path (Join-Path $scriptDir "..\..\files\share\bin")) {
    $SFROOT = (Resolve-Path (Join-Path $scriptDir "..\..\files")).Path
} else {
    $SFROOT = $scriptDir
}

if ([string]::IsNullOrWhiteSpace($EnvFile)) {
    $EnvFile = Join-Path $SFROOT "serverfiles.env"
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

function Protect-SensitiveFile([string]$filePath) {
    if (Test-Path $filePath) {
        $currentUser = [System.Security.Principal.WindowsIdentity]::GetCurrent().Name
        & icacls $filePath /inheritance:r /grant:r "${currentUser}:(R,W)" "SYSTEM:(F)" "Administrators:(F)" | Out-Null
    }
}

function Protect-OpenSslAes256([byte[]]$plainBytes, [string]$passphrase) {
    $salt = New-Object byte[] 8
    [System.Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($salt)
    $kdf = New-Object System.Security.Cryptography.Rfc2898DeriveBytes(
        [System.Text.Encoding]::UTF8.GetBytes($passphrase),
        $salt,
        100000,
        [System.Security.Cryptography.HashAlgorithmName]::SHA256
    )
    $key = $kdf.GetBytes(32)
    $iv  = $kdf.GetBytes(16)
    $aes = [System.Security.Cryptography.Aes]::Create()
    $aes.KeySize = 256
    $aes.BlockSize = 128
    $aes.Mode = [System.Security.Cryptography.CipherMode]::CBC
    $aes.Padding = [System.Security.Cryptography.PaddingMode]::PKCS7
    $aes.Key = $key
    $aes.IV  = $iv

    $ms = New-Object System.IO.MemoryStream
    $header = [System.Text.Encoding]::ASCII.GetBytes("Salted__")
    $ms.Write($header, 0, 8)
    $ms.Write($salt, 0, 8)
    $enc = $aes.CreateEncryptor()
    $cipherBytes = $enc.TransformFinalBlock($plainBytes, 0, $plainBytes.Length)
    $ms.Write($cipherBytes, 0, $cipherBytes.Length)
    return $ms.ToArray()
}

function Unprotect-OpenSslAes256([byte[]]$encBytes, [string]$passphrase) {
    if ($encBytes.Length -lt 16) {
        throw "Encrypted archive is truncated (< 16 bytes)."
    }
    $magic = [System.Text.Encoding]::ASCII.GetString($encBytes, 0, 8)
    if ($magic -ne "Salted__") {
        throw "Invalid OpenSSL header magic in archive."
    }
    $salt = New-Object byte[] 8
    [Array]::Copy($encBytes, 8, $salt, 0, 8)
    $kdf = New-Object System.Security.Cryptography.Rfc2898DeriveBytes(
        [System.Text.Encoding]::UTF8.GetBytes($passphrase),
        $salt,
        100000,
        [System.Security.Cryptography.HashAlgorithmName]::SHA256
    )
    $key = $kdf.GetBytes(32)
    $iv  = $kdf.GetBytes(16)
    $aes = [System.Security.Cryptography.Aes]::Create()
    $aes.KeySize = 256
    $aes.BlockSize = 128
    $aes.Mode = [System.Security.Cryptography.CipherMode]::CBC
    $aes.Padding = [System.Security.Cryptography.PaddingMode]::PKCS7
    $aes.Key = $key
    $aes.IV  = $iv
    $dec = $aes.CreateDecryptor()
    return $dec.TransformFinalBlock($encBytes, 16, $encBytes.Length - 16)
}

$cfg = Load-EnvMap $EnvFile
$dbHost = Get-ConfigValue $cfg "MT2_DB_HOST" "127.0.0.1"
$dbPort = Get-ConfigValue $cfg "MT2_DB_PORT" "3306"
$dbBinDir = Get-ConfigValue $cfg "MT2_DB_BIN_DIR" ""
$backupUser = Get-ConfigValue $cfg "MT2_BACKUP_DB_USER" "mt2backup"
$backupPass = Get-ConfigValue $cfg "MT2_BACKUP_DB_PASS" ""
$initUser = Get-ConfigValue $cfg "MT2_INIT_DB_USER" ""
$initPass = Get-ConfigValue $cfg "MT2_INIT_DB_PASS" ""
$passphrase = Get-ConfigValue $cfg "MT2_BACKUP_PASSPHRASE" ""
$retentionCount = [int](Get-ConfigValue $cfg "MT2_BACKUP_RETENTION_COUNT" "14")

if ([string]::IsNullOrWhiteSpace($BackupDir)) {
    $BackupDir = Get-ConfigValue $cfg "MT2_BACKUP_DIR" (Join-Path $SFROOT "backups")
}

if ([string]::IsNullOrWhiteSpace($passphrase) -or $passphrase.StartsWith("__CHANGE_ME_")) {
    throw "MT2_BACKUP_PASSPHRASE is unset or uses a placeholder."
}

$dumpCli = $null
$mysqlCli = $null
if (![string]::IsNullOrWhiteSpace($dbBinDir)) {
    foreach ($c in @("mariadb-dump.exe", "mysqldump.exe")) {
        $p = Join-Path $dbBinDir $c
        if (Test-Path $p) { $dumpCli = $p; break }
    }
    foreach ($c in @("mariadb.exe", "mysql.exe")) {
        $p = Join-Path $dbBinDir $c
        if (Test-Path $p) { $mysqlCli = $p; break }
    }
}
if (!$dumpCli) {
    $cmd = Get-Command mariadb-dump.exe, mysqldump.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($cmd) { $dumpCli = $cmd.Source }
}
if (!$mysqlCli) {
    $cmd = Get-Command mariadb.exe, mysql.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($cmd) { $mysqlCli = $cmd.Source }
}
if (!$dumpCli -or !$mysqlCli) {
    throw "mariadb-dump.exe or mariadb.exe not found in MT2_DB_BIN_DIR or PATH."
}

New-Item -ItemType Directory -Force -Path $BackupDir | Out-Null

if ($Action -eq "Backup") {
    if ([string]::IsNullOrWhiteSpace($backupPass) -or $backupPass.StartsWith("__CHANGE_ME_")) {
        throw "MT2_BACKUP_DB_PASS is unset or uses a placeholder."
    }
    $tmpCnf = Join-Path $BackupDir ".backup_client_${PID}.cnf"
    try {
        $cnfContent = "[client]`r`nhost=$dbHost`r`nport=$dbPort`r`nuser=$backupUser`r`npassword=`"$backupPass`"`r`n"
        [System.IO.File]::WriteAllText($tmpCnf, $cnfContent, [System.Text.Encoding]::ASCII)
        Protect-SensitiveFile $tmpCnf

        $stamp = (Get-Date).ToUniversalTime().ToString("yyyyMMdd_HHmmssZ")
        $outEnc = Join-Path $BackupDir "mt2_backup_${stamp}.sql.gz.enc"
        $outSha = "${outEnc}.sha256"

        $dumpArgs = @("--defaults-extra-file=$tmpCnf", "--single-transaction", "--quick", "--hex-blob", "--triggers")
        if ($Databases.Count -eq 1) {
            $dumpArgs += $Databases[0]
        } else {
            $dumpArgs += "--databases"
            $dumpArgs += $Databases
        }

        $rawSql = (& $dumpCli @dumpArgs | Out-String)
        if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($rawSql)) {
            throw "mariadb-dump failed with exit code $LASTEXITCODE"
        }

        $sqlBytes = [System.Text.Encoding]::UTF8.GetBytes($rawSql)
        $gzMs = New-Object System.IO.MemoryStream
        $gzStream = New-Object System.IO.Compression.GZipStream($gzMs, [System.IO.Compression.CompressionMode]::Compress, $true)
        $gzStream.Write($sqlBytes, 0, $sqlBytes.Length)
        $gzStream.Close()
        $gzBytes = $gzMs.ToArray()

        $encBytes = Protect-OpenSslAes256 -plainBytes $gzBytes -passphrase $passphrase
        [System.IO.File]::WriteAllBytes($outEnc, $encBytes)
        Protect-SensitiveFile $outEnc

        $hashHex = (Get-FileHash -Path $outEnc -Algorithm SHA256).Hash.ToLowerInvariant()
        $shaLine = "$hashHex  $(Split-Path -Leaf $outEnc)`n"
        [System.IO.File]::WriteAllText($outSha, $shaLine, [System.Text.Encoding]::ASCII)
        Protect-SensitiveFile $outSha

        Write-Host "[OK] Encrypted backup created: $outEnc (SHA256=$hashHex)"

        $existing = @(Get-ChildItem -Path $BackupDir -Filter "mt2_backup_*.sql.gz.enc" -File | Sort-Object LastWriteTime -Descending)
        if ($existing.Count -gt $retentionCount) {
            foreach ($old in ($existing | Select-Object -Skip $retentionCount)) {
                Remove-Item -Force $old.FullName -ErrorAction SilentlyContinue
                Remove-Item -Force "$($old.FullName).sha256" -ErrorAction SilentlyContinue
                Write-Host "[INFO] Pruned old backup exceeding retention ($retentionCount): $($old.FullName)"
            }
        }
    } finally {
        if (Test-Path $tmpCnf) { Remove-Item -Force $tmpCnf -ErrorAction SilentlyContinue }
    }
    return
}

if ($Action -eq "Restore") {
    if ([string]::IsNullOrWhiteSpace($RestoreArchive) -or !(Test-Path $RestoreArchive)) {
        throw "Restore archive not found: $RestoreArchive"
    }
    $shaFile = "${RestoreArchive}.sha256"
    if (!(Test-Path $shaFile)) {
        throw "Missing SHA-256 manifest: $shaFile"
    }
    $expectedHash = ((Get-Content $shaFile -TotalCount 1) -split '\s+')[0].Trim().ToLowerInvariant()
    $actualHash = (Get-FileHash -Path $RestoreArchive -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($expectedHash -ne $actualHash) {
        throw "SHA-256 integrity mismatch on $RestoreArchive! Expected=$expectedHash Actual=$actualHash"
    }
    Write-Host "[OK] Verified SHA-256 integrity of $RestoreArchive ($actualHash)"

    if ([string]::IsNullOrWhiteSpace($initUser) -or [string]::IsNullOrWhiteSpace($initPass)) {
        throw "Restore requires MT2_INIT_DB_USER and MT2_INIT_DB_PASS."
    }

    $encBytes = [System.IO.File]::ReadAllBytes($RestoreArchive)
    $gzBytes = Unprotect-OpenSslAes256 -encBytes $encBytes -passphrase $passphrase
    $inMs = New-Object System.IO.MemoryStream(,$gzBytes)
    $gzStream = New-Object System.IO.Compression.GZipStream($inMs, [System.IO.Compression.CompressionMode]::Decompress)
    $outMs = New-Object System.IO.MemoryStream
    $gzStream.CopyTo($outMs)
    $gzStream.Close()
    $sqlText = [System.Text.Encoding]::UTF8.GetString($outMs.ToArray())

    $tmpCnf = Join-Path $BackupDir ".restore_client_${PID}.cnf"
    try {
        $cnfContent = "[client]`r`nhost=$dbHost`r`nport=$dbPort`r`nuser=$initUser`r`npassword=`"$initPass`"`r`n"
        [System.IO.File]::WriteAllText($tmpCnf, $cnfContent, [System.Text.Encoding]::ASCII)
        Protect-SensitiveFile $tmpCnf

        $strictMode = "STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION"
        if (![string]::IsNullOrWhiteSpace($TargetDatabase)) {
            & $mysqlCli "--defaults-extra-file=$tmpCnf" -e "CREATE DATABASE IF NOT EXISTS ``$TargetDatabase`` CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;"
            if ($LASTEXITCODE -ne 0) { throw "Failed creating target database '$TargetDatabase'" }
            $sqlText | & $mysqlCli "--defaults-extra-file=$tmpCnf" $TargetDatabase --init-command="SET SESSION sql_mode='$strictMode';"
            if ($LASTEXITCODE -ne 0) { throw "Failed restoring SQL into '$TargetDatabase'" }
            Write-Host "[OK] Restored $RestoreArchive into isolated target database '$TargetDatabase'"
        } else {
            $sqlText | & $mysqlCli "--defaults-extra-file=$tmpCnf" --init-command="SET SESSION sql_mode='$strictMode';"
            if ($LASTEXITCODE -ne 0) { throw "Failed restoring SQL archive" }
            Write-Host "[OK] Restored $RestoreArchive into MariaDB instance"
        }
    } finally {
        if (Test-Path $tmpCnf) { Remove-Item -Force $tmpCnf -ErrorAction SilentlyContinue }
    }
}
