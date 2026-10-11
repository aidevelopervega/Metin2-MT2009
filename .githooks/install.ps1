# ==============================================================================
# Metin2 MT2009 - Yerel Git Kancaları Kurulum Betiği (PowerShell)
# Amacı: .githooks/ içindeki kancaları dallardan bağımsız kalıcı .git/hooks
# dizinine kurarak dal değişikliklerinde (checkout main vb.) korumanın
# kaybolmamasını sağlamak.
# ==============================================================================

$ErrorActionPreference = "Stop"

$RepoRoot = (& git rev-parse --show-toplevel 2>$null)
if (-not $RepoRoot) {
    throw "Git depo kök dizini bulunamadı!"
}

$SourceHooks = Join-Path $RepoRoot ".githooks"
$GitDir = (& git rev-parse --git-dir 2>$null)
if (-not $GitDir) {
    throw ".git dizini bulunamadı!"
}

if ([System.IO.Path]::IsPathRooted($GitDir)) {
    $TargetHooks = Join-Path $GitDir "hooks"
} else {
    $TargetHooks = Join-Path $RepoRoot "$GitDir\hooks"
}

if (-not (Test-Path $TargetHooks)) {
    New-Item -ItemType Directory -Path $TargetHooks -Force | Out-Null
}

# Kancaları kalıcı dizine kopyala
Copy-Item -Force "$SourceHooks\pre-commit" "$TargetHooks\pre-commit"
Copy-Item -Force "$SourceHooks\pre-push" "$TargetHooks\pre-push"

# core.hooksPath ayarını kalıcı mutlak yola yönlendir
$TargetHooksNormalized = (Resolve-Path $TargetHooks).Path.Replace("\", "/")
& git config core.hooksPath "$TargetHooksNormalized"

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host " Git kancaları kalıcı dizine kuruldu!" -ForegroundColor Green
Write-Host " Kaynak: $SourceHooks" -ForegroundColor Gray
Write-Host " Hedef (Kalıcı): $TargetHooksNormalized" -ForegroundColor Gray
Write-Host " core.hooksPath: $(git config core.hooksPath)" -ForegroundColor Green
Write-Host "==================================================" -ForegroundColor Cyan
