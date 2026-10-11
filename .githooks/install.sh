#!/bin/sh
# ==============================================================================
# Metin2 MT2009 - Yerel Git Kancaları Kurulum Betiği (POSIX / sh)
# Amacı: .githooks/ içindeki kancaları dallardan bağımsız kalıcı .git/hooks
# dizinine kurarak dal değişikliklerinde korumanın kaybolmamasını sağlamak.
# ==============================================================================

REPO_ROOT=$(git rev-parse --show-toplevel 2>/dev/null)
if [ -z "$REPO_ROOT" ]; then
    echo "HATA: Git depo kök dizini bulunamadı!" >&2
    exit 1
fi

GIT_DIR=$(git rev-parse --git-dir 2>/dev/null)
TARGET_HOOKS="$GIT_DIR/hooks"

mkdir -p "$TARGET_HOOKS"
cp -f "$REPO_ROOT/.githooks/pre-commit" "$TARGET_HOOKS/pre-commit"
cp -f "$REPO_ROOT/.githooks/pre-push" "$TARGET_HOOKS/pre-push"
chmod +x "$TARGET_HOOKS/pre-commit" "$TARGET_HOOKS/pre-push"

ABS_TARGET=$(cd "$TARGET_HOOKS" && pwd)
git config core.hooksPath "$ABS_TARGET"

echo "=================================================="
echo "Git kancaları kalıcı dizine kuruldu!"
echo "Hedef (Kalıcı): $ABS_TARGET"
echo "core.hooksPath: $(git config core.hooksPath)"
echo "=================================================="
