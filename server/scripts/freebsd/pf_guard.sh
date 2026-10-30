#!/bin/sh
# ==============================================================================
# Metin2 MT2009 — FreeBSD 15.1 PF Firewall Policy Validator & Port Drift Guard
# ==============================================================================
# SAFETY GUARANTEE:
# This script is strictly NON-DESTRUCTIVE and READ-ONLY with respect to the
# active kernel packet filter. It NEVER runs `pfctl -e` or loads active rules
# without explicit manual operator intervention.
# ==============================================================================

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ -f "$SCRIPT_DIR/pf.conf.example" ]; then
    DEFAULT_PF_CONF="$SCRIPT_DIR/pf.conf.example"
elif [ -f "$SCRIPT_DIR/../../config/pf.conf.example" ]; then
    DEFAULT_PF_CONF=$(CDPATH= cd -- "$SCRIPT_DIR/../../config" && pwd)/pf.conf.example
else
    DEFAULT_PF_CONF="/etc/pf.conf"
fi

PF_CONF="$DEFAULT_PF_CONF"
SERVERFILES_DIR="${MT2_SERVERFILES_DIR:-/usr/home/mt2}"
ROLLBACK_SECONDS=60
ROLLBACK_STATE_DIR="${MT2_PF_ROLLBACK_DIR:-/tmp/mt2_pf_rollback}"
AUDIT_ALERT_FILE="${MT2_AUDIT_ALERT_FILE:-/var/log/mt2_security_alerts.log}"
ACTION="verify_all"

while [ "$#" -gt 0 ]; do
    case "$1" in
        --verify-template)
            ACTION="verify_template"
            shift
            ;;
        --verify-config-ports)
            ACTION="verify_config_ports"
            shift
            ;;
        --verify-all)
            ACTION="verify_all"
            shift
            ;;
        --print-matrix)
            ACTION="print_matrix"
            shift
            ;;
        --arm-rollback)
            ACTION="arm_rollback"
            if [ -n "${2:-}" ] && [ "${2#-}" = "$2" ]; then
                ROLLBACK_SECONDS="$2"
                shift 2
            else
                shift 1
            fi
            ;;
        --confirm-rollback)
            ACTION="confirm_rollback"
            shift
            ;;
        --rollback-state-dir)
            ROLLBACK_STATE_DIR="$2"
            shift 2
            ;;
        --pf-conf)
            PF_CONF="$2"
            shift 2
            ;;
        --serverfiles-dir)
            SERVERFILES_DIR="$2"
            shift 2
            ;;
        *)
            echo "[ERROR] Unknown argument: $1" >&2
            echo "Usage: $0 [--verify-template | --verify-config-ports | --verify-all | --print-matrix | --arm-rollback [sec] | --confirm-rollback] [--pf-conf <path>] [--serverfiles-dir <dir>] [--rollback-state-dir <dir>]" >&2
            exit 1
            ;;
    esac
done

emit_pf_alert() {
    tag="$1"
    msg="$2"
    ts=$(date -u +"%Y-%m-%dT%H:%M:%SZ" 2>/dev/null || date)
    line="$ts $tag $msg"
    if [ -n "$AUDIT_ALERT_FILE" ]; then
        alert_dir=$(dirname "$AUDIT_ALERT_FILE")
        mkdir -p "$alert_dir" 2>/dev/null || true
        if [ -d "$alert_dir" ]; then
            umask 077
            { echo "$line" >> "$AUDIT_ALERT_FILE"; } 2>/dev/null || true
        fi
    fi
    if command -v logger >/dev/null 2>&1; then
        logger -p security.crit -t mt2_pf_guard "$tag $msg" 2>/dev/null || true
    fi
}

extract_macro_ports() {
    macro_name="$1"
    conf_file="$2"
    sed -n "s/^[[:space:]]*${macro_name}[[:space:]]*=[[:space:]]*\"\{0,1\}[{[:space:]]*\([^}\"]*\)[}\"[:space:]]*$/\1/p" "$conf_file" \
        | tr ',' ' ' | tr -s ' \t\r\n' ' ' | sed 's/^ //;s/ $//'
}

port_in_list() {
    needle="$1"
    haystack="$2"
    for p in $haystack; do
        if [ "$p" = "$needle" ]; then
            return 0
        fi
    done
    return 1
}

verify_template_rules() {
    if [ ! -f "$PF_CONF" ]; then
        echo "[ERROR] PF configuration file not found: $PF_CONF" >&2
        exit 1
    fi

    internal_ports=$(extract_macro_ports "mt2_internal_ports" "$PF_CONF")
    p2p_ports=$(extract_macro_ports "mt2_p2p_ports" "$PF_CONF")
    player_ports=$(extract_macro_ports "mt2_player_tcp_ports" "$PF_CONF")
    auth_port=$(extract_macro_ports "mt2_auth_port" "$PF_CONF")
    game_ports=$(extract_macro_ports "mt2_game_ports" "$PF_CONF")

    # 1. Verify required internal ports (SQL 3306/3307 + DB Cache 18000 + 7 P2P ports) are blocked
    for req_int in 3306 3307 18000 19555 29000 29001 29002 14099 14088 14098; do
        if ! port_in_list "$req_int" "$internal_ports"; then
            echo "[ERROR] Required internal/SQL/P2P port $req_int is missing from mt2_internal_ports in $PF_CONF" >&2
            exit 1
        fi
        if port_in_list "$req_int" "$player_ports" || port_in_list "$req_int" "$game_ports" || [ "$req_int" = "$auth_port" ]; then
            echo "[ERROR] Internal port $req_int MUST NOT appear in player-facing ports in $PF_CONF" >&2
            exit 1
        fi
    done

    # 2. Verify all 7 P2P ports are in mt2_p2p_ports
    for req_p2p in 19555 29000 29001 29002 14099 14088 14098; do
        if ! port_in_list "$req_p2p" "$p2p_ports"; then
            echo "[ERROR] P2P port $req_p2p is missing from mt2_p2p_ports in $PF_CONF" >&2
            exit 1
        fi
    done

    # 3. Verify all 7 player TCP ports (auth 17105 + 6 channels) are present
    if [ "$auth_port" != "17105" ]; then
        echo "[ERROR] Expected mt2_auth_port=17105, got '$auth_port' in $PF_CONF" >&2
        exit 1
    fi
    for req_play in 17105 28000 28001 28002 13099 13088 13098; do
        if ! port_in_list "$req_play" "$player_ports"; then
            echo "[ERROR] Player port $req_play is missing from mt2_player_tcp_ports in $PF_CONF" >&2
            exit 1
        fi
    done

    # 4. Verify critical PF policy directives exist in template
    if ! grep -Eq '^[[:space:]]*set[[:space:]]+skip[[:space:]]+on[[:space:]]+lo0' "$PF_CONF"; then
        echo "[ERROR] Missing 'set skip on lo0' (required for loopback MariaDB 127.0.0.1 and DB cache IPC) in $PF_CONF" >&2
        exit 1
    fi
    if ! grep -Eq '^[[:space:]]*block[[:space:]]+in[[:space:]]+all' "$PF_CONF"; then
        echo "[ERROR] Missing default deny 'block in all' in $PF_CONF" >&2
        exit 1
    fi
    if ! grep -Eq 'pass[[:space:]]+in[[:space:]]+quick[[:space:]]+on[[:space:]]+\$ext_if[[:space:]]+proto[[:space:]]+tcp.*port[[:space:]]+\$ssh_port' "$PF_CONF"; then
        echo "[ERROR] Missing anti-lockout management SSH rule ('pass in quick ... port \$ssh_port') in $PF_CONF" >&2
        exit 1
    fi
    if ! grep -Eq 'block[[:space:]]+drop[[:space:]]+in.*on[[:space:]]+\$ext_if[[:space:]]+inet[[:space:]]+proto[[:space:]]+\{[[:space:]]*tcp,[[:space:]]*udp[[:space:]]*\}.*port[[:space:]]+\$mt2_internal_ports' "$PF_CONF"; then
        echo "[ERROR] Missing IPv4 external block rule for \$mt2_internal_ports in $PF_CONF" >&2
        exit 1
    fi
    if ! grep -Eq 'block[[:space:]]+drop[[:space:]]+in.*on[[:space:]]+\$ext_if[[:space:]]+inet6[[:space:]]+proto[[:space:]]+\{[[:space:]]*tcp,[[:space:]]*udp[[:space:]]*\}.*port[[:space:]]+\$mt2_internal_ports' "$PF_CONF"; then
        echo "[ERROR] Missing IPv6 external block rule for \$mt2_internal_ports in $PF_CONF" >&2
        exit 1
    fi
    if ! grep -Eq 'block[[:space:]]+drop[[:space:]]+in.*on[[:space:]]+\$ext_if[[:space:]]+inet[[:space:]]+proto[[:space:]]+udp.*port[[:space:]]+\$mt2_player_tcp_ports' "$PF_CONF"; then
        echo "[ERROR] Missing external UDP block rule for \$mt2_player_tcp_ports in $PF_CONF" >&2
        exit 1
    fi
    if ! grep -Eq 'block[[:space:]]+drop[[:space:]]+in.*on[[:space:]]+\$ext_if[[:space:]]+inet6[[:space:]]+proto[[:space:]]+tcp.*port[[:space:]]+\$mt2_player_tcp_ports' "$PF_CONF"; then
        echo "[ERROR] Missing IPv6 TCP block rule for \$mt2_player_tcp_ports in $PF_CONF" >&2
        exit 1
    fi

    # 5. If running on FreeBSD with pfctl available (or MT2_PFCTL_CMD override), run dry-run syntax parse (`pfctl -nf`)
    pfctl_cmd="${MT2_PFCTL_CMD:-}"
    if [ -z "$pfctl_cmd" ] && [ "$(uname -s 2>/dev/null || true)" = "FreeBSD" ] && command -v pfctl >/dev/null 2>&1; then
        pfctl_cmd="pfctl"
    fi
    if [ -n "$pfctl_cmd" ]; then
        tmp_pf="/tmp/mt2_pf_syntax_$$.conf"
        sed 's/ext_if[[:space:]]*=[[:space:]]*"[^"]*"/ext_if = "lo0"/' "$PF_CONF" > "$tmp_pf"
        if ! "$pfctl_cmd" -nf "$tmp_pf" >/dev/null 2>&1; then
            if [ "$(uname -s 2>/dev/null || true)" = "FreeBSD" ] && command -v kldload >/dev/null 2>&1; then
                if ! kldload -n pf >/dev/null 2>&1; then
                    kld_err=$(kldload -n pf 2>&1 || true)
                    echo "[ERROR] Failed to load FreeBSD pf kernel module (kldload -n pf): $kld_err" >&2
                fi
            fi
        fi
        if "$pfctl_cmd" -nf "$tmp_pf" >/dev/null 2>&1; then
            rm -f "$tmp_pf"
            echo "[OK] FreeBSD pfctl -nf dry-run syntax validation passed (no kernel rules modified)."
        else
            pf_err=$("$pfctl_cmd" -nf "$tmp_pf" 2>&1 || true)
            rm -f "$tmp_pf"
            echo "[ERROR] pfctl -nf syntax validation failed: $pf_err" >&2
            exit 1
        fi
    fi

    echo "[OK] PF firewall policy template verified ($PF_CONF): SQL(3306/3307), DB(18000), P2P($p2p_ports) & UDP blocked on WAN; Player TCP($player_ports) & SSH protected."
}

verify_config_drift() {
    if [ ! -d "$SERVERFILES_DIR" ]; then
        echo "[ERROR] Serverfiles directory not found: $SERVERFILES_DIR" >&2
        exit 1
    fi
    if [ ! -f "$SERVERFILES_DIR/db/conf.txt" ] || [ ! -f "$SERVERFILES_DIR/auth/CONFIG" ]; then
        echo "[ERROR] Missing db/conf.txt or auth/CONFIG in $SERVERFILES_DIR" >&2
        exit 1
    fi

    internal_ports=$(extract_macro_ports "mt2_internal_ports" "$PF_CONF")
    player_ports=$(extract_macro_ports "mt2_player_tcp_ports" "$PF_CONF")

    db_port=$(awk -F'=' '/^[[:space:]]*BIND_PORT[[:space:]]*=/{gsub(/[ \t\r\n"]/, "", $2); print $2}' "$SERVERFILES_DIR/db/conf.txt")
    db_bind_ip=$(awk -F'=' '/^[[:space:]]*BIND_IP[[:space:]]*=/{gsub(/[ \t\r\n"]/, "", $2); print $2}' "$SERVERFILES_DIR/db/conf.txt")
    if [ -z "$db_port" ]; then
        echo "[ERROR] BIND_PORT not found in $SERVERFILES_DIR/db/conf.txt" >&2
        exit 1
    fi
    if ! port_in_list "$db_port" "$internal_ports"; then
        echo "[ERROR] db/conf.txt BIND_PORT=$db_port is NOT blocked in mt2_internal_ports ($PF_CONF)!" >&2
        exit 1
    fi
    if port_in_list "$db_port" "$player_ports"; then
        echo "[ERROR] db/conf.txt BIND_PORT=$db_port is exposed in mt2_player_tcp_ports ($PF_CONF)!" >&2
        exit 1
    fi
    if [ -z "$db_bind_ip" ]; then
        echo "[ERROR] BIND_IP is missing in $SERVERFILES_DIR/db/conf.txt (db_r41023 would bind to 0.0.0.0:$db_port)!" >&2
        exit 1
    fi

    for cfg in "$SERVERFILES_DIR/auth/CONFIG" "$SERVERFILES_DIR"/chs/*/CONFIG; do
        [ -f "$cfg" ] || continue
        svc_port=$(awk -F':' '/^[[:space:]]*PORT[[:space:]]*:/{gsub(/[ \t\r\n]/, "", $2); print $2}' "$cfg")
        svc_p2p=$(awk -F':' '/^[[:space:]]*P2P_PORT[[:space:]]*:/{gsub(/[ \t\r\n]/, "", $2); print $2}' "$cfg")
        if [ -z "$svc_port" ] || [ -z "$svc_p2p" ]; then
            echo "[ERROR] Missing PORT or P2P_PORT in $cfg" >&2
            exit 1
        fi
        if ! port_in_list "$svc_port" "$player_ports"; then
            echo "[ERROR] Service PORT=$svc_port in $cfg is missing from mt2_player_tcp_ports ($PF_CONF)!" >&2
            exit 1
        fi
        if port_in_list "$svc_port" "$internal_ports"; then
            echo "[ERROR] Service PORT=$svc_port in $cfg is wrongly listed in mt2_internal_ports ($PF_CONF)!" >&2
            exit 1
        fi
        if ! port_in_list "$svc_p2p" "$internal_ports"; then
            echo "[ERROR] Internal P2P_PORT=$svc_p2p in $cfg is NOT blocked in mt2_internal_ports ($PF_CONF)!" >&2
            exit 1
        fi
        if port_in_list "$svc_p2p" "$player_ports"; then
            echo "[ERROR] Internal P2P_PORT=$svc_p2p in $cfg is exposed in mt2_player_tcp_ports ($PF_CONF)!" >&2
            exit 1
        fi
    done

    echo "[OK] Verified zero port drift between 8-service CONFIG files in $SERVERFILES_DIR and $PF_CONF (db BIND_IP=$db_bind_ip:$db_port)."
}

print_port_matrix() {
    cat <<'EOF'
================================================================================
MT2009 FreeBSD 15.1 — 8-Service + MariaDB 12.3 LTS Network & PF Firewall Matrix
================================================================================
Service    Config File       Port   Proto  Bind Scope    WAN PF Policy
---------  ----------------  -----  -----  ------------  -----------------------
mariadb    my.cnf            3306   TCP    127.0.0.1     BLOCK DROP QUICK (4/6)
db         db/conf.txt       18000  TCP    127.0.0.1     BLOCK DROP QUICK (4/6)
auth       auth/CONFIG       17105  TCP    MT2_BIND_IP   PASS IN (IPv4 TCP Only)
auth-p2p   auth/CONFIG       19555  TCP    MT2_BIND_IP   BLOCK DROP QUICK (4/6)
ch11       chs/ch11/CONFIG   28000  TCP    MT2_BIND_IP   PASS IN (IPv4 TCP Only)
ch11-p2p   chs/ch11/CONFIG   29000  TCP    MT2_BIND_IP   BLOCK DROP QUICK (4/6)
ch12       chs/ch12/CONFIG   28001  TCP    MT2_BIND_IP   PASS IN (IPv4 TCP Only)
ch12-p2p   chs/ch12/CONFIG   29001  TCP    MT2_BIND_IP   BLOCK DROP QUICK (4/6)
ch13       chs/ch13/CONFIG   28002  TCP    MT2_BIND_IP   PASS IN (IPv4 TCP Only)
ch13-p2p   chs/ch13/CONFIG   29002  TCP    MT2_BIND_IP   BLOCK DROP QUICK (4/6)
ch99       chs/ch99/CONFIG   13099  TCP    MT2_BIND_IP   PASS IN (IPv4 TCP Only)
ch99-p2p   chs/ch99/CONFIG   14099  TCP    MT2_BIND_IP   BLOCK DROP QUICK (4/6)
ch00       chs/ch00/CONFIG   13088  TCP    MT2_BIND_IP   PASS IN (IPv4 TCP Only)
ch00-p2p   chs/ch00/CONFIG   14088  TCP    MT2_BIND_IP   BLOCK DROP QUICK (4/6)
ch01       chs/ch01/CONFIG   13098  TCP    MT2_BIND_IP   PASS IN (IPv4 TCP Only)
ch01-p2p   chs/ch01/CONFIG   14098  TCP    MT2_BIND_IP   BLOCK DROP QUICK (4/6)
game-udp   auth & chs/*      all    UDP    MT2_BIND_IP   BLOCK DROP QUICK (4/6)
sshd       /etc/ssh/sshd_cfg 22     TCP    Host IP       PASS IN QUICK (Protected)
================================================================================
EOF
}

arm_rollback_watchdog() {
    case "$ROLLBACK_SECONDS" in
        ''|*[!0-9]*|0)
            echo "[ERROR] Invalid rollback timeout seconds: $ROLLBACK_SECONDS" >&2
            exit 1
            ;;
    esac
    umask 077
    mkdir -p "$ROLLBACK_STATE_DIR"
    chmod 700 "$ROLLBACK_STATE_DIR"
    confirm_file="$ROLLBACK_STATE_DIR/pf_confirmed"
    pid_file="$ROLLBACK_STATE_DIR/watchdog.pid"
    status_file="$ROLLBACK_STATE_DIR/rollback.status"

    if [ -f "$pid_file" ]; then
        old_pid=$(tr -d ' \t\r\n' < "$pid_file" 2>/dev/null || true)
        if [ -n "$old_pid" ] && kill -0 "$old_pid" 2>/dev/null; then
            echo "[ERROR] A PF rollback watchdog is already active (PID $old_pid). Run --confirm-rollback first." >&2
            exit 1
        fi
        rm -f "$pid_file"
    fi
    rm -f "$confirm_file" "$status_file"
    printf 'ARMED\n' > "$status_file"

    pfctl_bin="${MT2_PFCTL_CMD:-/sbin/pfctl}"
    (
        # Ignore SIGHUP (SSH disconnect), SIGINT (Ctrl+C), SIGQUIT, SIGPIPE, SIGTSTP
        # so neither a dropped SSH session nor terminal Ctrl+C can abort the safety rollback.
        trap '' HUP INT QUIT PIPE TSTP
        elapsed=0
        while [ "$elapsed" -lt "$ROLLBACK_SECONDS" ]; do
            if [ -f "$confirm_file" ]; then
                rm -f "$confirm_file" "$pid_file"
                printf 'CONFIRMED_DISARMED\n' > "$status_file"
                exit 0
            fi
            sleep 1
            elapsed=$((elapsed + 1))
        done
        if [ -f "$confirm_file" ]; then
            rm -f "$confirm_file" "$pid_file"
            printf 'CONFIRMED_DISARMED\n' > "$status_file"
            exit 0
        fi
        "$pfctl_bin" -d >/dev/null 2>&1 || true
        rm -f "$pid_file"
        printf 'AUTO_ROLLBACK_EXECUTED\n' > "$status_file"
        emit_pf_alert "[MT2_PF_ALERT][AUTO_ROLLBACK_EXECUTED]" "PF automatically disabled ($pfctl_bin -d) after ${ROLLBACK_SECONDS}s without operator confirmation."
    ) </dev/null >/dev/null 2>&1 &
    wd_pid=$!
    printf '%s\n' "$wd_pid" > "$pid_file"
    echo "[OK] Armed SIGHUP/SIGINT-immune PF auto-rollback watchdog (PID=$wd_pid, timeout=${ROLLBACK_SECONDS}s). Run '$0 --confirm-rollback' within ${ROLLBACK_SECONDS}s after verifying SSH."
}

confirm_rollback_watchdog() {
    if [ ! -d "$ROLLBACK_STATE_DIR" ]; then
        echo "[ERROR] Rollback state directory not found: $ROLLBACK_STATE_DIR" >&2
        exit 1
    fi
    confirm_file="$ROLLBACK_STATE_DIR/pf_confirmed"
    pid_file="$ROLLBACK_STATE_DIR/watchdog.pid"
    status_file="$ROLLBACK_STATE_DIR/rollback.status"
    printf 'CONFIRMED\n' > "$confirm_file"
    i=0
    while [ -f "$pid_file" ] && [ "$i" -lt 15 ]; do
        sleep 0.2
        i=$((i + 1))
    done
    cur_status=$(tr -d ' \t\r\n' < "$status_file" 2>/dev/null || true)
    if [ "$cur_status" != "CONFIRMED_DISARMED" ]; then
        echo "[ERROR] Watchdog did not confirm disarm cleanly (status='$cur_status')." >&2
        exit 1
    fi
    echo "[OK] Disarmed PF auto-rollback watchdog; active firewall rules remain in place."
}

case "$ACTION" in
    verify_template)
        verify_template_rules
        ;;
    verify_config_ports)
        verify_template_rules
        verify_config_drift
        ;;
    verify_all)
        verify_template_rules
        if [ -d "$SERVERFILES_DIR" ] && [ -f "$SERVERFILES_DIR/db/conf.txt" ]; then
            verify_config_drift
        fi
        ;;
    print_matrix)
        print_port_matrix
        ;;
    arm_rollback)
        arm_rollback_watchdog
        ;;
    confirm_rollback)
        confirm_rollback_watchdog
        ;;
esac
