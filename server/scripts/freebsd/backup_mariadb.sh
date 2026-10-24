#!/bin/sh
# ==============================================================================
# Metin2 MT2009 — MariaDB 12.3 LTS Encrypted Backup & Isolated Restore Utility
# ==============================================================================
# Features:
# 1. Least-Privilege Backup Account (`MT2_BACKUP_DB_USER`, default `mt2backup`):
#    Uses ONLY `SELECT, SHOW VIEW, TRIGGER, LOCK TABLES` via a chmod 0600 temp
#    client config (`--defaults-extra-file`) — no password on process CLI.
# 2. InnoDB Consistent Snapshot + Cache Consistency Notice:
#    Uses `mariadb-dump --single-transaction --quick --hex-blob`.
#    NOTE on `db_r41023` Write-Back Cache (`CPlayerTableCache` / `CItemCache`):
#    - Online Hot Backup: SQL-consistent InnoDB snapshot; active online players
#      have up to 180s-600s (`SAVE_EVENT_SECOND_CYCLE` / `HASH_PLAYER_LIFE_SEC`)
#      of un-flushed in-memory cache state in `db_r41023`.
#    - Zero-RPO Maintenance Backup: Run `stop.sh` first so `db_r41023` flushes
#      all dirty player/item caches to MariaDB before dumping.
# 3. Encryption & Integrity:
#    Compresses with `gzip` and encrypts with `openssl enc -aes-256-cbc -pbkdf2 -iter 100000`.
#    Generates `<archive>.sha256` and verifies SHA-256 prior to any restore.
# 4. Retention Rotation:
#    Retains the newest `MT2_BACKUP_RETENTION_COUNT` (default: 14) encrypted backups.
# ==============================================================================

set -e

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SFROOT="$SCRIPT_DIR"
ENV_FILE="$SFROOT/serverfiles.env"
ACTION="backup"
BACKUP_DIR="${MT2_BACKUP_DIR:-/var/backups/mt2_mariadb}"
DATABASES="account common log player world"
RESTORE_ARCHIVE=""
RESTORE_TARGET_DB=""

while [ "$#" -gt 0 ]; do
    case "$1" in
        --backup)
            ACTION="backup"
            shift 1
            ;;
        --restore)
            ACTION="restore"
            RESTORE_ARCHIVE="$2"
            shift 2
            ;;
        --target-db)
            RESTORE_TARGET_DB="$2"
            shift 2
            ;;
        --databases)
            DATABASES="$2"
            shift 2
            ;;
        --backup-dir)
            BACKUP_DIR="$2"
            shift 2
            ;;
        --env-file)
            ENV_FILE="$2"
            shift 2
            ;;
        *)
            echo "[ERROR] Unknown argument: $1" >&2
            exit 1
            ;;
    esac
done

if [ -f "$ENV_FILE" ]; then
    while IFS='=' read -r k v || [ -n "$k" ]; do
        k_trimmed=$(printf '%s' "$k" | tr -d ' \t\r\n')
        case "$k_trimmed" in
            ""|\#*) continue ;;
        esac
        v_trimmed=$(printf '%s' "$v" | sed 's/^[[:space:]]*//;s/[[:space:]\r]*$//')
        eval "cur_val=\${$k_trimmed:-}"
        if [ -z "$cur_val" ]; then
            export "$k_trimmed=$v_trimmed"
        fi
    done < "$ENV_FILE"
fi

MT2_DB_HOST="${MT2_DB_HOST:-127.0.0.1}"
MT2_DB_PORT="${MT2_DB_PORT:-3306}"
MT2_BACKUP_DB_USER="${MT2_BACKUP_DB_USER:-mt2backup}"
MT2_BACKUP_DB_PASS="${MT2_BACKUP_DB_PASS:-}"
MT2_INIT_DB_USER="${MT2_INIT_DB_USER:-}"
MT2_INIT_DB_PASS="${MT2_INIT_DB_PASS:-}"
MT2_BACKUP_PASSPHRASE="${MT2_BACKUP_PASSPHRASE:-}"
MT2_BACKUP_RETENTION_COUNT="${MT2_BACKUP_RETENTION_COUNT:-14}"

if [ -z "$MT2_BACKUP_PASSPHRASE" ] || [ "${MT2_BACKUP_PASSPHRASE#__CHANGE_ME_}" != "$MT2_BACKUP_PASSPHRASE" ]; then
    echo "[ERROR] MT2_BACKUP_PASSPHRASE is not set or uses a placeholder." >&2
    exit 1
fi

DUMP_CLI=$(which mariadb-dump 2>/dev/null || which mysqldump 2>/dev/null || true)
DB_CLI=$(which mariadb 2>/dev/null || which mysql 2>/dev/null || true)
SHA_CLI=$(which sha256 2>/dev/null || which sha256sum 2>/dev/null || true)

if [ -z "$DUMP_CLI" ] || [ -z "$DB_CLI" ]; then
    echo "[ERROR] mariadb-dump/mysqldump or mariadb/mysql CLI not found in PATH." >&2
    exit 1
fi

compute_sha256() {
    f="$1"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$f" | awk '{print $1}'
    elif command -v sha256 >/dev/null 2>&1; then
        sha256 -q "$f"
    else
        openssl dgst -sha256 "$f" | awk '{print $NF}'
    fi
}

TMP_CNF=""
TMP_PASS=""
cleanup() {
    [ -n "$TMP_CNF" ] && rm -f "$TMP_CNF"
    [ -n "$TMP_PASS" ] && rm -f "$TMP_PASS"
}
trap cleanup EXIT INT TERM

umask 077
mkdir -p "$BACKUP_DIR"
chmod 700 "$BACKUP_DIR"

TMP_PASS="$BACKUP_DIR/.enc_pass_$$.tmp"
printf '%s\n' "$MT2_BACKUP_PASSPHRASE" > "$TMP_PASS"
chmod 600 "$TMP_PASS"

if [ "$ACTION" = "backup" ]; then
    if [ -z "$MT2_BACKUP_DB_PASS" ] || [ "${MT2_BACKUP_DB_PASS#__CHANGE_ME_}" != "$MT2_BACKUP_DB_PASS" ]; then
        echo "[ERROR] MT2_BACKUP_DB_PASS is not set or uses a placeholder." >&2
        exit 1
    fi

    TMP_CNF="$BACKUP_DIR/.backup_client_$$.cnf"
    cat > "$TMP_CNF" <<EOF
[client]
host=$MT2_DB_HOST
port=$MT2_DB_PORT
user=$MT2_BACKUP_DB_USER
password="$MT2_BACKUP_DB_PASS"
EOF
    chmod 600 "$TMP_CNF"

    STAMP=$(date -u +"%Y%m%d_%H%M%SZ")
    OUT_ENC="$BACKUP_DIR/mt2_backup_${STAMP}.sql.gz.enc"
    OUT_SHA="${OUT_ENC}.sha256"

    if [ -f "$SFROOT/db/pid" ] && kill -0 "$(cat "$SFROOT/db/pid" 2>/dev/null)" 2>/dev/null; then
        echo "[INFO] db_r41023 is currently running (online hot backup mode: InnoDB snapshot is atomic; in-memory CPlayerTableCache/CItemCache RPO window <= 180-600s)."
    else
        echo "[INFO] db_r41023 is stopped (cold maintenance backup mode: 0-second cache RPO)."
    fi

    set -- $DATABASES
    if [ "$#" -eq 1 ]; then
        "$DUMP_CLI" "--defaults-extra-file=$TMP_CNF" \
            --single-transaction --quick --hex-blob --triggers \
            "$1" | gzip -c | openssl enc -aes-256-cbc -pbkdf2 -iter 100000 -salt \
            -pass "file:$TMP_PASS" -out "$OUT_ENC"
    else
        "$DUMP_CLI" "--defaults-extra-file=$TMP_CNF" \
            --single-transaction --quick --hex-blob --triggers \
            --databases "$@" | gzip -c | openssl enc -aes-256-cbc -pbkdf2 -iter 100000 -salt \
            -pass "file:$TMP_PASS" -out "$OUT_ENC"
    fi

    chmod 600 "$OUT_ENC"
    HASH_VAL=$(compute_sha256 "$OUT_ENC")
    printf '%s  %s\n' "$HASH_VAL" "$(basename "$OUT_ENC")" > "$OUT_SHA"
    chmod 600 "$OUT_SHA"

    echo "[OK] Encrypted backup created: $OUT_ENC (SHA256=$HASH_VAL)"

    # Retention rotation
    COUNT=0
    for f in $(ls -1t "$BACKUP_DIR"/mt2_backup_*.sql.gz.enc 2>/dev/null); do
        COUNT=$((COUNT + 1))
        if [ "$COUNT" -gt "$MT2_BACKUP_RETENTION_COUNT" ]; then
            rm -f "$f" "${f}.sha256"
            echo "[INFO] Pruned old backup exceeding retention ($MT2_BACKUP_RETENTION_COUNT): $f"
        fi
    done
    exit 0
fi

if [ "$ACTION" = "restore" ]; then
    if [ -z "$RESTORE_ARCHIVE" ] || [ ! -f "$RESTORE_ARCHIVE" ]; then
        echo "[ERROR] Restore archive not found: $RESTORE_ARCHIVE" >&2
        exit 1
    fi
    SHA_FILE="${RESTORE_ARCHIVE}.sha256"
    if [ ! -f "$SHA_FILE" ]; then
        echo "[ERROR] Missing SHA-256 manifest: $SHA_FILE" >&2
        exit 1
    fi
    EXPECTED_HASH=$(awk '{print $1}' "$SHA_FILE")
    ACTUAL_HASH=$(compute_sha256 "$RESTORE_ARCHIVE")
    if [ "$EXPECTED_HASH" != "$ACTUAL_HASH" ]; then
        echo "[ERROR] SHA-256 mismatch on $RESTORE_ARCHIVE! Expected=$EXPECTED_HASH Actual=$ACTUAL_HASH" >&2
        exit 1
    fi
    echo "[OK] Verified SHA-256 integrity of $RESTORE_ARCHIVE ($ACTUAL_HASH)"

    if [ -z "$MT2_INIT_DB_USER" ] || [ -z "$MT2_INIT_DB_PASS" ]; then
        echo "[ERROR] Restore requires MT2_INIT_DB_USER and MT2_INIT_DB_PASS." >&2
        exit 1
    fi

    TMP_CNF="$BACKUP_DIR/.restore_client_$$.cnf"
    cat > "$TMP_CNF" <<EOF
[client]
host=$MT2_DB_HOST
port=$MT2_DB_PORT
user=$MT2_INIT_DB_USER
password="$MT2_INIT_DB_PASS"
EOF
    chmod 600 "$TMP_CNF"

    STRICT_MODE="STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION"
    if [ -n "$RESTORE_TARGET_DB" ]; then
        "$DB_CLI" "--defaults-extra-file=$TMP_CNF" \
            -e "CREATE DATABASE IF NOT EXISTS \`${RESTORE_TARGET_DB}\` CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;"
        openssl enc -d -aes-256-cbc -pbkdf2 -iter 100000 -pass "file:$TMP_PASS" -in "$RESTORE_ARCHIVE" \
            | gzip -dc \
            | "$DB_CLI" "--defaults-extra-file=$TMP_CNF" "$RESTORE_TARGET_DB" \
                --init-command="SET SESSION sql_mode='$STRICT_MODE';"
        echo "[OK] Restored $RESTORE_ARCHIVE into isolated target database '$RESTORE_TARGET_DB'"
    else
        openssl enc -d -aes-256-cbc -pbkdf2 -iter 100000 -pass "file:$TMP_PASS" -in "$RESTORE_ARCHIVE" \
            | gzip -dc \
            | "$DB_CLI" "--defaults-extra-file=$TMP_CNF" \
                --init-command="SET SESSION sql_mode='$STRICT_MODE';"
        echo "[OK] Restored $RESTORE_ARCHIVE into MariaDB instance"
    fi
    exit 0
fi
