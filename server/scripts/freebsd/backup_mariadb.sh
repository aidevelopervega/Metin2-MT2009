#!/bin/sh
# ==============================================================================
# Metin2 MT2009 — MariaDB 12.3 LTS Authenticated Encrypted Backup & Restore (v2)
# ==============================================================================
# Security & Consistency Guarantees:
# 1. Least-Privilege Backup Account (`MT2_BACKUP_DB_USER`, default `mt2backup`):
#    Uses ONLY `SELECT, SHOW VIEW, TRIGGER, LOCK TABLES, EVENT` via a chmod 0600
#    temporary client config (`--defaults-extra-file`) — no password on CLI.
# 2. Storage Engine & Cache Consistency Verification:
#    - Queries `information_schema.TABLES` before dumping to verify whether all
#      tables use `ENGINE=InnoDB`. If any non-InnoDB table exists, emits
#      `[MT2_BACKUP_WARN][NON_INNODB_ENGINE]` because `--single-transaction`
#      only guarantees MVCC snapshot consistency for InnoDB tables.
#    - Dumps all tables, views, triggers, routines, and events via
#      `--single-transaction --quick --hex-blob --triggers --routines --events`.
#    - `db_r41023` Write-Back Cache (`CPlayerTableCache` / `CItemCache`):
#      * Online (Hot) Backup: InnoDB snapshot is SQL-atomic, but in-memory dirty
#        caches flush asynchronously (180s-600s); worst-case RPO is:
#        RPO_hot <= T_backup_interval + max(SAVE_EVENT_SECOND_CYCLE, HASH_PLAYER_LIFE_SEC).
#      * Controlled Offline (Cold) Backup (after `stop.sh`): `db_r41023` flushes
#        all dirty caches on graceful shutdown (cache lag = 0s at backup instant).
# 3. Authenticated Encryption (Encrypt-then-MAC v2 + Legacy v1 Compatibility):
#    - Compresses with `gzip` and encrypts with `openssl enc -aes-256-cbc -pbkdf2 -iter 100000 -salt`.
#    - Writes `<archive>.sha256` containing:
#        Line 1: `<sha256_hex>  <archive_basename>` (v1 compatible)
#        Line 2: `HMAC-SHA256(v2)=<keyed_hmac_hex>` computed over
#                `v2|<archive_basename>|<sha256_hex>|<raw_ciphertext>` using
#                `MT2_BACKUP_HMAC_SECRET` (or domain-separated key from passphrase).
#    - Restore verifies BOTH `SHA-256` AND keyed `HMAC-SHA256(v2)` before
#      decrypting, detecting tampering even if an attacker modifies both the
#      archive and the `.sha256` file. Legacy v1 archives (without Line 2)
#      require explicit `--allow-legacy-v1-sha256`.
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
ALLOW_LEGACY_V1=0
INSTALL_CRON=0
CRON_FILE=""

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
        --allow-legacy-v1-sha256)
            ALLOW_LEGACY_V1=1
            shift 1
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
        --install-cron)
            INSTALL_CRON=1
            if [ -n "${2:-}" ] && [ "${2#-}" = "$2" ]; then
                CRON_FILE="$2"
                shift 2
            else
                shift 1
            fi
            ;;
        --cron-file)
            CRON_FILE="$2"
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
MT2_BACKUP_HMAC_SECRET="${MT2_BACKUP_HMAC_SECRET:-}"
MT2_BACKUP_RETENTION_COUNT="${MT2_BACKUP_RETENTION_COUNT:-14}"
MT2_BACKUP_MAX_TOTAL_MB="${MT2_BACKUP_MAX_TOTAL_MB:-10240}"
MT2_MIN_FREE_DISK_MB="${MT2_MIN_FREE_DISK_MB:-1024}"
AUDIT_ALERT_FILE="${MT2_AUDIT_ALERT_FILE:-/var/log/mt2_security_alerts.log}"

emit_backup_alert() {
    tag="$1"
    msg="$2"
    ts=$(date -u +"%Y-%m-%dT%H:%M:%SZ" 2>/dev/null || date)
    line="$ts $tag $msg"
    echo "$line" >&2
    if [ -n "$AUDIT_ALERT_FILE" ]; then
        mkdir -p "$(dirname "$AUDIT_ALERT_FILE")" 2>/dev/null || true
        umask 077
        echo "$line" >> "$AUDIT_ALERT_FILE" 2>/dev/null || true
    fi
    if command -v logger >/dev/null 2>&1; then
        logger -p security.crit -t mt2_backup "$tag $msg" 2>/dev/null || true
    fi
}

if [ "$INSTALL_CRON" -eq 1 ]; then
    target_cron="${CRON_FILE:-/usr/local/etc/cron.d/mt2_backup}"
    umask 077
    mkdir -p "$(dirname "$target_cron")"
    cat > "$target_cron" <<EOF
# Metin2 MT2009 — Automated Audit Guard & Encrypted MariaDB 12.3 LTS Backup Schedule
SHELL=/bin/sh
PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin

# Every 5 minutes: enforce audit permissions/chflags, check disk capacity, check spool syserr, and forward audit logs
*/5 * * * * root "$SCRIPT_DIR/audit_guard.sh" --enforce-perms && "$SCRIPT_DIR/audit_guard.sh" --check-capacity && "$SCRIPT_DIR/audit_guard.sh" --check-spool-syserr

# Every 6 hours: take authenticated encrypted MariaDB 12.3 LTS backup via least-privilege mt2backup user
0 */6 * * * root "$SCRIPT_DIR/backup_mariadb.sh" --backup --env-file "$ENV_FILE"
EOF
    chmod 600 "$target_cron"
    echo "[OK] Installed cron schedule template at $target_cron"
    exit 0
fi

if [ -z "$MT2_BACKUP_PASSPHRASE" ] || [ "${MT2_BACKUP_PASSPHRASE#__CHANGE_ME_}" != "$MT2_BACKUP_PASSPHRASE" ]; then
    emit_backup_alert "[MT2_BACKUP_ALERT][CONFIG_ERROR]" "MT2_BACKUP_PASSPHRASE is not set or uses a placeholder."
    exit 1
fi

DUMP_CLI=$(which mariadb-dump 2>/dev/null || which mysqldump 2>/dev/null || true)
DB_CLI=$(which mariadb 2>/dev/null || which mysql 2>/dev/null || true)

if [ -z "$DUMP_CLI" ] || [ -z "$DB_CLI" ]; then
    emit_backup_alert "[MT2_BACKUP_ALERT][CLI_MISSING]" "mariadb-dump/mysqldump or mariadb/mysql CLI not found in PATH."
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

derive_hmac_key() {
    if [ -n "$MT2_BACKUP_HMAC_SECRET" ] && [ "${MT2_BACKUP_HMAC_SECRET#__CHANGE_ME_}" = "$MT2_BACKUP_HMAC_SECRET" ]; then
        printf '%s' "$MT2_BACKUP_HMAC_SECRET"
    else
        printf '%s' "MT2_HMAC_V2_DOMAIN_KEY:${MT2_BACKUP_PASSPHRASE}" | openssl dgst -sha256 | awk '{print $NF}'
    fi
}

compute_backup_hmac_v2() {
    enc_file="$1"
    sha_hex="$2"
    hmac_key="$3"
    base_name=$(basename "$enc_file")
    (
        printf 'v2|%s|%s|' "$base_name" "$sha_hex"
        cat "$enc_file"
    ) | openssl dgst -sha256 -hmac "$hmac_key" | awk '{print $NF}'
}

TMP_CNF=""
TMP_PASS=""
TMP_GZ=""
TMP_SQL=""
cleanup() {
    [ -n "$TMP_CNF" ] && rm -f "$TMP_CNF"
    [ -n "$TMP_PASS" ] && rm -f "$TMP_PASS"
    [ -n "$TMP_GZ" ] && rm -f "$TMP_GZ"
    [ -n "$TMP_SQL" ] && rm -f "$TMP_SQL"
}
trap cleanup EXIT INT TERM

umask 077
mkdir -p "$BACKUP_DIR"
chmod 700 "$BACKUP_DIR"

TMP_PASS="$BACKUP_DIR/.enc_pass_$$.tmp"
printf '%s\n' "$MT2_BACKUP_PASSPHRASE" > "$TMP_PASS"
chmod 600 "$TMP_PASS"
HMAC_KEY=$(derive_hmac_key)

if [ "$ACTION" = "backup" ]; then
    if [ -z "$MT2_BACKUP_DB_PASS" ] || [ "${MT2_BACKUP_DB_PASS#__CHANGE_ME_}" != "$MT2_BACKUP_DB_PASS" ]; then
        emit_backup_alert "[MT2_BACKUP_ALERT][CONFIG_ERROR]" "MT2_BACKUP_DB_PASS is not set or uses a placeholder."
        exit 1
    fi

    # Check available free disk space on BACKUP_DIR filesystem (in MiB)
    if command -v df >/dev/null 2>&1; then
        free_mb=$(df -Pm "$BACKUP_DIR" 2>/dev/null | awk 'NR==2 {print $4}' | tr -d ' \t\r\n')
        case "$free_mb" in
            ''|*[!0-9]*) ;;
            *)
                if [ "$free_mb" -lt "$MT2_MIN_FREE_DISK_MB" ]; then
                    emit_backup_alert "[MT2_BACKUP_ALERT][DISK_FREE_LOW]" "Available disk space ${free_mb}MB on $BACKUP_DIR is below minimum threshold ${MT2_MIN_FREE_DISK_MB}MB."
                    exit 2
                fi
                ;;
        esac
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

    # Storage engine consistency audit across target schemas
    db_in_list=""
    for db_item in $DATABASES; do
        if [ -z "$db_in_list" ]; then
            db_in_list="'$db_item'"
        else
            db_in_list="$db_in_list,'$db_item'"
        fi
    done
    non_innodb=$("$DB_CLI" "--defaults-extra-file=$TMP_CNF" -N -s \
        -e "SELECT CONCAT(TABLE_SCHEMA,'.',TABLE_NAME,'(',ENGINE,')') FROM information_schema.TABLES WHERE TABLE_SCHEMA IN ($db_in_list) AND TABLE_TYPE='BASE TABLE' AND ENGINE <> 'InnoDB';" 2>/dev/null || true)
    if [ -n "$non_innodb" ]; then
        emit_backup_alert "[MT2_BACKUP_WARN][NON_INNODB_ENGINE]" "Non-InnoDB tables detected ($non_innodb); --single-transaction does NOT guarantee point-in-time consistency for non-InnoDB tables!"
    fi

    STAMP=$(date -u +"%Y%m%d_%H%M%SZ")
    OUT_ENC="$BACKUP_DIR/mt2_backup_${STAMP}.sql.gz.enc"
    OUT_SHA="${OUT_ENC}.sha256"
    TMP_SQL="$BACKUP_DIR/.dump_$$.sql"
    TMP_GZ="$BACKUP_DIR/.dump_$$.sql.gz"

    if [ -f "$SFROOT/db/pid" ] && kill -0 "$(cat "$SFROOT/db/pid" 2>/dev/null)" 2>/dev/null; then
        echo "[INFO] Mode: ONLINE HOT BACKUP (db_r41023 running). InnoDB SQL snapshot is atomic; application cross-table cache state is bounded by RPO_hot <= T_backup_interval + 600s (HASH_PLAYER_LIFE_SEC)."
    else
        echo "[INFO] Mode: CONTROLLED OFFLINE COLD BACKUP (db_r41023 stopped). Cache lag = 0s at backup instant."
    fi

    set -- $DATABASES
    if [ "$#" -eq 1 ]; then
        if ! "$DUMP_CLI" "--defaults-extra-file=$TMP_CNF" \
            --single-transaction --quick --hex-blob --triggers --routines --events \
            "$1" > "$TMP_SQL"; then
            emit_backup_alert "[MT2_BACKUP_ALERT][DUMP_FAILED]" "mariadb-dump failed for schema '$1'."
            exit 1
        fi
    else
        if ! "$DUMP_CLI" "--defaults-extra-file=$TMP_CNF" \
            --single-transaction --quick --hex-blob --triggers --routines --events \
            --databases "$@" > "$TMP_SQL"; then
            emit_backup_alert "[MT2_BACKUP_ALERT][DUMP_FAILED]" "mariadb-dump failed for schemas '$*'."
            exit 1
        fi
    fi

    if [ ! -s "$TMP_SQL" ]; then
        emit_backup_alert "[MT2_BACKUP_ALERT][DUMP_EMPTY]" "mariadb-dump produced an empty SQL file."
        exit 1
    fi

    gzip -c "$TMP_SQL" > "$TMP_GZ"
    rm -f "$TMP_SQL"
    if ! openssl enc -aes-256-cbc -pbkdf2 -iter 100000 -salt \
        -pass "file:$TMP_PASS" -in "$TMP_GZ" -out "$OUT_ENC"; then
        emit_backup_alert "[MT2_BACKUP_ALERT][ENCRYPT_FAILED]" "OpenSSL AES-256-CBC encryption failed."
        exit 1
    fi
    rm -f "$TMP_GZ"

    chmod 600 "$OUT_ENC"
    HASH_VAL=$(compute_sha256 "$OUT_ENC")
    HMAC_VAL=$(compute_backup_hmac_v2 "$OUT_ENC" "$HASH_VAL" "$HMAC_KEY")
    {
        printf '%s  %s\n' "$HASH_VAL" "$(basename "$OUT_ENC")"
        printf 'HMAC-SHA256(v2)=%s\n' "$HMAC_VAL"
    } > "$OUT_SHA"
    chmod 600 "$OUT_SHA"

    echo "[OK] Authenticated encrypted backup (v2) created: $OUT_ENC (SHA256=$HASH_VAL, HMAC-SHA256=$HMAC_VAL)"

    # Retention rotation by count and total size cap
    COUNT=0
    TOTAL_BYTES=0
    MAX_BYTES=$((MT2_BACKUP_MAX_TOTAL_MB * 1024 * 1024))
    for f in $(ls -1t "$BACKUP_DIR"/mt2_backup_*.sql.gz.enc 2>/dev/null); do
        COUNT=$((COUNT + 1))
        f_bytes=$(wc -c < "$f" | tr -d ' \t\r\n')
        TOTAL_BYTES=$((TOTAL_BYTES + f_bytes))
        if [ "$COUNT" -gt "$MT2_BACKUP_RETENTION_COUNT" ] || { [ "$COUNT" -gt 1 ] && [ "$TOTAL_BYTES" -gt "$MAX_BYTES" ]; }; then
            rm -f "$f" "${f}.sha256"
            echo "[INFO] Pruned old backup exceeding retention policy (count=$MT2_BACKUP_RETENTION_COUNT, max_mb=$MT2_BACKUP_MAX_TOTAL_MB): $f"
        fi
    done
    exit 0
fi

if [ "$ACTION" = "restore" ]; then
    if [ -z "$RESTORE_ARCHIVE" ] || [ ! -f "$RESTORE_ARCHIVE" ]; then
        emit_backup_alert "[MT2_BACKUP_ALERT][RESTORE_ERROR]" "Restore archive not found: $RESTORE_ARCHIVE"
        exit 1
    fi
    SHA_FILE="${RESTORE_ARCHIVE}.sha256"
    if [ ! -f "$SHA_FILE" ]; then
        emit_backup_alert "[MT2_BACKUP_ALERT][TAMPER_DETECTED]" "Missing integrity manifest: $SHA_FILE"
        exit 1
    fi
    EXPECTED_HASH=$(awk 'NR==1 {print $1}' "$SHA_FILE" | tr -d ' \t\r\n')
    ACTUAL_HASH=$(compute_sha256 "$RESTORE_ARCHIVE")
    if [ "$EXPECTED_HASH" != "$ACTUAL_HASH" ]; then
        emit_backup_alert "[MT2_BACKUP_ALERT][TAMPER_DETECTED]" "SHA-256 mismatch on $RESTORE_ARCHIVE! Expected=$EXPECTED_HASH Actual=$ACTUAL_HASH"
        exit 1
    fi

    EXPECTED_HMAC=$(sed -n 's/^HMAC-SHA256(v2)=//p' "$SHA_FILE" | tr -d ' \t\r\n')
    if [ -n "$EXPECTED_HMAC" ]; then
        ACTUAL_HMAC=$(compute_backup_hmac_v2 "$RESTORE_ARCHIVE" "$ACTUAL_HASH" "$HMAC_KEY")
        if [ "$EXPECTED_HMAC" != "$ACTUAL_HMAC" ]; then
            emit_backup_alert "[MT2_BACKUP_ALERT][TAMPER_DETECTED]" "Keyed HMAC-SHA256(v2) verification FAILED on $RESTORE_ARCHIVE! Archive or manifest was modified without the secret HMAC key."
            exit 1
        fi
        echo "[OK] Verified SHA-256 ($ACTUAL_HASH) and Keyed HMAC-SHA256(v2) authenticity of $RESTORE_ARCHIVE"
    else
        if [ "$ALLOW_LEGACY_V1" -ne 1 ]; then
            emit_backup_alert "[MT2_BACKUP_ALERT][TAMPER_DETECTED]" "Manifest $SHA_FILE lacks HMAC-SHA256(v2) tag! Refusing unauthenticated restore (pass --allow-legacy-v1-sha256 only for trusted legacy v1 backups)."
            exit 1
        fi
        echo "[WARN] Legacy v1 manifest accepted via --allow-legacy-v1-sha256 (SHA256=$ACTUAL_HASH; no keyed HMAC)."
    fi

    if [ -z "$MT2_INIT_DB_USER" ] || [ -z "$MT2_INIT_DB_PASS" ]; then
        emit_backup_alert "[MT2_BACKUP_ALERT][RESTORE_ERROR]" "Restore requires MT2_INIT_DB_USER and MT2_INIT_DB_PASS."
        exit 1
    fi

    TMP_GZ="$BACKUP_DIR/.restore_$$.sql.gz"
    TMP_SQL="$BACKUP_DIR/.restore_$$.sql"
    if ! openssl enc -d -aes-256-cbc -pbkdf2 -iter 100000 -pass "file:$TMP_PASS" -in "$RESTORE_ARCHIVE" -out "$TMP_GZ" 2>/dev/null; then
        emit_backup_alert "[MT2_BACKUP_ALERT][DECRYPT_FAILED]" "AES-256-CBC decryption failed on $RESTORE_ARCHIVE (wrong passphrase or corrupted ciphertext)."
        exit 1
    fi
    if ! gzip -t "$TMP_GZ" 2>/dev/null; then
        emit_backup_alert "[MT2_BACKUP_ALERT][DECOMPRESS_FAILED]" "Decompressed payload is not valid gzip data on $RESTORE_ARCHIVE."
        exit 1
    fi
    gzip -dc "$TMP_GZ" | sed 's/\/\*!50017 DEFINER=`[^`]*`@`[^`]*`\*\///g' > "$TMP_SQL"
    rm -f "$TMP_GZ"
    if [ ! -s "$TMP_SQL" ]; then
        emit_backup_alert "[MT2_BACKUP_ALERT][RESTORE_EMPTY]" "Decrypted SQL payload is empty on $RESTORE_ARCHIVE."
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
        "$DB_CLI" "--defaults-extra-file=$TMP_CNF" --default-character-set=utf8mb4 \
            -e "CREATE DATABASE IF NOT EXISTS \`${RESTORE_TARGET_DB}\` CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;"
        "$DB_CLI" "--defaults-extra-file=$TMP_CNF" --default-character-set=utf8mb4 "$RESTORE_TARGET_DB" \
            --init-command="SET SESSION sql_mode='$STRICT_MODE';" < "$TMP_SQL"
        echo "[OK] Restored $RESTORE_ARCHIVE into isolated target database '$RESTORE_TARGET_DB'"
    else
        "$DB_CLI" "--defaults-extra-file=$TMP_CNF" --default-character-set=utf8mb4 \
            --init-command="SET SESSION sql_mode='$STRICT_MODE';" < "$TMP_SQL"
        echo "[OK] Restored $RESTORE_ARCHIVE into MariaDB instance"
    fi
    rm -f "$TMP_SQL"
    exit 0
fi
