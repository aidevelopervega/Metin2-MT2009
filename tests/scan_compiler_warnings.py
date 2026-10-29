#!/usr/bin/env python3
"""
Permanent, account-independent GCC extended warning audit tool for MT2009 Server.
Replaces session-bound scripts:
  - scan_current_warnings.py
  - run_extended_warnings.py
  - rescan_modified_tus.py

Separates first-party diagnostics (server/src/, tests/) from third-party SDK
headers (extern/) and reports both raw and deduplicated counts by warning flag.
"""

from __future__ import annotations

import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor, as_completed
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys

EXTENDED_FLAGS = [
    "-Wformat=2",
    "-Wshadow",
    "-Wconversion",
    "-Wsign-conversion",
    "-Wcast-align",
    "-Wpointer-arith",
    "-Wundef",
    "-Wstrict-aliasing",
]

DIAG_RE = re.compile(
    r"^(?P<file>[A-Za-z]:[^:\n]+|[^:\n]+):(?P<line>\d+):(?P<col>\d+):\s+"
    r"(?P<kind>warning|error):\s+(?P<msg>.*?)(?:\s+\[(?P<flag>-W[^\]]+)\])?$"
)


def detect_supported_flags(compiler: str, flags: list[str]) -> list[str]:
    supported: list[str] = []
    for flag in flags:
        proc = subprocess.run(
            [compiler, "-x", "c++", "-std=c++20", flag, "-fsyntax-only", "-"],
            input="",
            text=True,
            capture_output=True,
            check=False,
        )
        if proc.returncode == 0:
            supported.append(flag)
    return supported


def get_modified_basenames(repo_root: Path) -> set[str]:
    names: set[str] = set()
    for cmd in (
        ["git", "diff", "--name-only", "HEAD"],
        ["git", "diff", "--name-only", "HEAD~1..HEAD"],
    ):
        proc = subprocess.run(
            cmd, cwd=str(repo_root), text=True, capture_output=True, check=False
        )
        if proc.returncode == 0:
            for line in proc.stdout.splitlines():
                line = line.strip()
                if line:
                    names.add(Path(line).name.lower())
    return names


def classify_path(file_path: str, repo_root: Path) -> str:
    norm = file_path.replace("\\", "/").lower()
    root_norm = str(repo_root).replace("\\", "/").lower()
    if norm.startswith(root_norm):
        rel = norm[len(root_norm) :].lstrip("/")
    else:
        rel = norm
    if rel.startswith("extern/") or "/extern/" in norm:
        return "extern"
    if rel.startswith("server/src/") or "/server/src/" in norm or rel.startswith("tests/"):
        return "server/src"
    return "other"


def run_one_tu(entry: dict, extra_flags: list[str]) -> tuple[str, int, str]:
    directory = entry["directory"]
    src_file = entry["file"]
    if "arguments" in entry:
        args = list(entry["arguments"])
    else:
        args = shlex.split(entry["command"], posix=(os.name != "nt"))

    cleaned: list[str] = []
    skip_next = False
    for idx, tok in enumerate(args):
        if skip_next:
            skip_next = False
            continue
        if idx > 0 and tok.startswith('"') and tok.endswith('"') and len(tok) >= 2:
            tok = tok[1:-1]
        if tok in ("-o", "-MF", "-MT", "-MQ"):
            skip_next = True
            continue
        if tok in ("-c", "-MD", "-MMD"):
            continue
        cleaned.append(tok)

    cmd = cleaned + extra_flags + ["-fsyntax-only"]
    proc = subprocess.run(
        cmd, cwd=directory, text=True, capture_output=True, encoding="utf-8", errors="replace", check=False
    )
    return src_file, proc.returncode, (proc.stderr or "") + (proc.stdout or "")


def main() -> int:
    parser = argparse.ArgumentParser(description="MT2009 Extended GCC Warning Scanner")
    parser.add_argument(
        "--build-dir",
        default="build/windows-gcc-ucrt",
        help="Path to CMake build directory containing compile_commands.json",
    )
    parser.add_argument(
        "--filter",
        default="",
        help="Substring filter on source file path (e.g. 'server/src/game' or 'char.cpp')",
    )
    parser.add_argument(
        "--modified-only",
        action="store_true",
        help="Only scan translation units modified in working tree or last commit",
    )
    parser.add_argument(
        "--jobs",
        type=int,
        default=6,
        help="Max parallel compiler processes (default: 6 to respect 50%% CPU cap)",
    )
    parser.add_argument(
        "--json-out",
        default="",
        help="Optional path to write structured JSON report",
    )
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    comp_db_path = (repo_root / args.build_dir / "compile_commands.json").resolve()
    if not comp_db_path.is_file():
        print(f"ERROR: compile_commands.json not found at {comp_db_path}", file=sys.stderr)
        return 2

    entries = json.loads(comp_db_path.read_text(encoding="utf-8"))
    first_party_entries = []
    mod_names = get_modified_basenames(repo_root) if args.modified_only else set()

    for entry in entries:
        f_norm = entry.get("file", "").replace("\\", "/")
        if "/extern/" in f_norm.lower():
            continue
        if args.filter and args.filter.lower() not in f_norm.lower():
            continue
        if args.modified_only and Path(f_norm).name.lower() not in mod_names:
            continue
        first_party_entries.append(entry)

    if not first_party_entries:
        print("No matching first-party translation units to scan.")
        return 0

    first_cmd = first_party_entries[0].get("command", "")
    compiler = shlex.split(first_cmd, posix=(os.name != "nt"))[0].strip('"')
    supported_flags = detect_supported_flags(compiler, EXTENDED_FLAGS)

    print(f"=== MT2009 Extended GCC Warning Audit ===")
    print(f"Compiler          : {compiler}")
    print(f"Supported Flags   : {' '.join(supported_flags)}")
    print(f"Translation Units : {len(first_party_entries)} (workers={args.jobs})")

    raw_by_flag: Counter[str] = Counter()
    unique_seen: set[tuple[str, str, str, str]] = set()
    unique_by_flag: Counter[str] = Counter()
    first_party_unique_by_flag: Counter[str] = Counter()
    extern_unique_by_flag: Counter[str] = Counter()
    errors_count = 0
    sample_first_party: list[str] = []

    with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        futures = [
            pool.submit(run_one_tu, entry, supported_flags)
            for entry in first_party_entries
        ]
        for fut in as_completed(futures):
            src_file, rc, output = fut.result()
            if rc != 0:
                errors_count += 1
            for line in output.splitlines():
                m = DIAG_RE.match(line.strip())
                if not m:
                    continue
                if m.group("kind") == "error":
                    continue
                f_path = m.group("file")
                line_no = m.group("line")
                col_no = m.group("col")
                flag = m.group("flag") or "-Wunknown"
                raw_by_flag[flag] += 1
                key = (f_path.replace("\\", "/").lower(), line_no, col_no, flag)
                if key not in unique_seen:
                    unique_seen.add(key)
                    unique_by_flag[flag] += 1
                    scope = classify_path(f_path, repo_root)
                    if scope == "server/src":
                        first_party_unique_by_flag[flag] += 1
                        if len(sample_first_party) < 25:
                            sample_first_party.append(
                                f"{f_path}:{line_no}:{col_no}: [{flag}] {m.group('msg')}"
                            )
                    elif scope == "extern":
                        extern_unique_by_flag[flag] += 1

    print("\n--- Warning Summary by Flag (Deduplicated Unique Locations) ---")
    print(f"{'Flag':<28} {'UniqueTotal':>12} {'server/src':>12} {'extern/':>12} {'RawTotal':>12}")
    print("-" * 80)
    all_flags = sorted(unique_by_flag.keys(), key=lambda k: (-first_party_unique_by_flag[k], -unique_by_flag[k], k))
    for flg in all_flags:
        print(
            f"{flg:<28} {unique_by_flag[flg]:>12} "
            f"{first_party_unique_by_flag[flg]:>12} "
            f"{extern_unique_by_flag[flg]:>12} "
            f"{raw_by_flag[flg]:>12}"
        )
    print("-" * 80)
    print(
        f"{'TOTAL':<28} {sum(unique_by_flag.values()):>12} "
        f"{sum(first_party_unique_by_flag.values()):>12} "
        f"{sum(extern_unique_by_flag.values()):>12} "
        f"{sum(raw_by_flag.values()):>12}"
    )
    print(f"Syntax-only TU errors: {errors_count}")

    if args.json_out:
        out_data = {
            "compiler": compiler,
            "supported_flags": supported_flags,
            "translation_units_scanned": len(first_party_entries),
            "syntax_errors": errors_count,
            "raw_by_flag": dict(raw_by_flag),
            "unique_by_flag": dict(unique_by_flag),
            "first_party_unique_by_flag": dict(first_party_unique_by_flag),
            "extern_unique_by_flag": dict(extern_unique_by_flag),
            "sample_first_party": sample_first_party,
        }
        Path(args.json_out).write_text(json.dumps(out_data, indent=2), encoding="utf-8")
        print(f"Saved JSON report to: {args.json_out}")

    return 1 if errors_count > 0 else 0


if __name__ == "__main__":
    raise SystemExit(main())
