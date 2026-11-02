#!/usr/bin/env python3
"""MT2009 Autopilot v2: revision-scoped evidence and resumable local checkpoint; no network/git/subprocess."""
import argparse
import json
import os
import tempfile
from pathlib import Path
from datetime import datetime, timezone

STATUSES = ("ACTIVE", "PASS", "FAILED", "BLOCKED", "PENDING", "UNVERIFIED", "NOT_RUN")
GATES = (
    "workstream",
    "windows_build",
    "windows_tests",
    "freebsd_ci",
    "client_syntax",
    "client_objects",
    "client_link",
    "client_link_preflight",
    "gameplay",
    "python3_runtime",
    "python3_sdk_verified",
    "python3_client_embedded",
    "python3_real_modules",
    "miniaudio_dependency_acquired",
    "miniaudio_real_api_compiled",
    "miniaudio_runtime_tested",
    "miniaudio_production_adapter",
    "real_eterpack_audio",
    "physical_audio_output",
    "miles_adapter_integrated",
    "d3d9ex_api_test",
    "d3d9ex_renderer_integration",
    "sdk_inventory_complete",
    "unused_sdks_removed",
    "server_client_sdk_versions_unified",
    "sdk_x64_headers_verified",
    "sdk_x64_binaries_verified",
    "sdk_runtime_tests",
    "sdk_cleanup_independently_verified",
    "d3dx9_x64_acquired",
    "d3dx9_x64_compiled",
    "d3dx9_x64_runtime",
    "stb_image_acquired_and_locked",
    "server_markimage_stb_migrated",
    "client_markimage_stb_migrated",
    "guild_mark_stb_regression_tests",
    "devil_removed",
    "security",
)


def now():
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def location():
    root = Path.cwd().resolve()
    if not (root / "AGENTS.md").is_file() or not (root / "PROJECT_STATE.md").is_file():
        raise SystemExit("Run from MT2009 repository root (AGENTS.md + PROJECT_STATE.md required).")
    return root / "scratch" / "autopilot" / "state.json"


def load(path):
    if not path.is_file():
        return None
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def save(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, filename = tempfile.mkstemp(prefix=".state-", suffix=".json", dir=str(path.parent))
    try:
        with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as stream:
            json.dump(data, stream, ensure_ascii=False, indent=2)
            stream.write("\n")
        os.replace(filename, path)
    finally:
        if os.path.exists(filename):
            os.unlink(filename)


def upgrade(data):
    if data.get("schema", 0) < 2:
        data["schema"] = 2
    data.setdefault("campaign_status", "ACTIVE")
    data.setdefault("gates", {})
    data.setdefault("history", [])
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sp = parser.add_subparsers(dest="command", required=True)
    init = sp.add_parser("init")
    init.add_argument("--goal", required=True)
    init.add_argument("--head", default="UNKNOWN")
    up = sp.add_parser("update")
    up.add_argument("--task", required=True)
    up.add_argument("--status", required=True, choices=STATUSES)
    up.add_argument("--gate", default="workstream", choices=GATES)
    up.add_argument("--head", default="UNKNOWN")
    up.add_argument("--verified-head", default="")
    up.add_argument("--run-id", default="")
    up.add_argument("--evidence", default="")
    up.add_argument("--next", default="")
    up.add_argument("--skill", action="append", default=[])
    up.add_argument("--completed", default="")
    up.add_argument("--pending", default="")
    audit = sp.add_parser("audit")
    audit.add_argument("--head", required=True)
    sp.add_parser("show")
    args = parser.parse_args()
    path = location()
    state = load(path)
    if args.command == "show":
        print(json.dumps(state or {"status": "NO_CHECKPOINT"}, ensure_ascii=False, indent=2))
        return
    if args.command == "init":
        if state:
            if state.get("goal") != args.goal:
                raise SystemExit("Existing goal differs; preserve checkpoint and review manually before replacement.")
            upgrade(state)
            state["updated_at"] = now()
            save(path, state)
            print("Existing checkpoint preserved and migrated to schema v2.")
        else:
            state = {"schema":2,"goal":args.goal,"campaign_status":"ACTIVE","created_at":now(),"updated_at":now(),"initial_head":args.head,"current":None,"gates":{},"history":[]}
            save(path, state)
            print(str(path))
        return
    if state is None:
        raise SystemExit("Missing checkpoint; run init first. Do not delete prior checkpoint.")
    upgrade(state)
    if args.command == "audit":
        gates = {}
        for name, item in state.get("gates", {}).items():
            if name == "freebsd_ci":
                verified = item.get("verified_head") or item.get("head") if item.get("status") == "PASS" else ""
                gates[name] = {"status":item.get("status"),"verified_head":verified,"run_id":item.get("run_id"),"matches_current_head":bool(verified and verified == args.head)}
            else:
                gates[name] = {"status":item.get("status"),"verified_head":item.get("verified_head"),"matches_current_head":(item.get("verified_head") == args.head if item.get("verified_head") else None)}
        print(json.dumps({"campaign_status":state["campaign_status"],"current_head":args.head,"last_task":state.get("current"),"gates":gates,"alert":"Older FreeBSD CI PASS is not proof for a newer source revision" if gates.get("freebsd_ci",{}).get("status") == "PASS" and not gates["freebsd_ci"].get("matches_current_head") else ""},ensure_ascii=False,indent=2))
        return
    if args.gate == "freebsd_ci" and args.status == "PASS" and not (args.run_id and args.verified_head):
        raise SystemExit("FreeBSD PASS requires --run-id AND --verified-head (actual CI headSha).")
    if args.status == "PASS" and not args.evidence.strip():
        raise SystemExit("PASS requires explicit real evidence.")
    if args.verified_head and args.status == "PASS" and args.verified_head != args.head:
        raise SystemExit("PASS verified-head must match --head; record old CI on its own SHA instead.")
    item = {"at":now(),"gate":args.gate,"task":args.task,"status":args.status,"head":args.head,"verified_head":args.verified_head,"run_id":args.run_id,"evidence":args.evidence[:1200],"next":args.next[:600],"skills":args.skill[:12],"completed":args.completed[:400],"pending":args.pending[:400]}
    state["current"] = item
    state["gates"][args.gate] = item
    state["history"] = (state.get("history",[])+[item])[-200:]
    state["updated_at"] = item["at"]
    state["campaign_status"] = "ACTIVE"  # A per-gate PASS never closes the native x64 goal.
    save(path, state)
    print(json.dumps(item,ensure_ascii=False,indent=2))


if __name__ == "__main__":
    main()
