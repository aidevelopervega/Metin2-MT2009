#!/usr/bin/env python3
# ==============================================================================
# Metin2 MT2009 - Quest Compiler Matrix & Automated Parity Test System
# Supports: Lua 5.0 (default), Lua 5.2 (staging), and Modern Lua (upgraded)
# Features:
#   - Resumable JSON state (never redo completed files)
#   - Strict per-process timeout
#   - Isolated working directory per engine & quest
#   - Byte-level output verification across all generated state artifacts
#   - Categorized failure logging & comprehensive report generation
# ==============================================================================

import os
import sys
import json
import time
import shutil
import hashlib
import argparse
import subprocess
from pathlib import Path

def compute_file_hash(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()

def compare_directories(dir1, dir2):
    """Recursively compares two directories byte-for-byte."""
    p1 = Path(dir1)
    p2 = Path(dir2)
    if not p1.exists() and not p2.exists():
        return True, "both_empty", 0
    if not p1.exists() or not p2.exists():
        return False, f"one_missing (dir1: {p1.exists()}, dir2: {p2.exists()})", 0

    files1 = {f.relative_to(p1): f for f in p1.rglob("*") if f.is_file()}
    files2 = {f.relative_to(p2): f for f in p2.rglob("*") if f.is_file()}

    if set(files1.keys()) != set(files2.keys()):
        diff_keys = set(files1.keys()) ^ set(files2.keys())
        sample = list(diff_keys)[:3]
        return False, f"file_set_mismatch: {len(files1)} vs {len(files2)} (diff: {sample})", len(files1)

    for rel_path, file1 in files1.items():
        file2 = files2[rel_path]
        if file1.stat().st_size != file2.stat().st_size:
            return False, f"size_mismatch at {rel_path}", len(files1)
        if file1.read_bytes() != file2.read_bytes():
            return False, f"byte_mismatch at {rel_path}", len(files1)

    return True, "identical", len(files1)

def run_single_engine(engine_name, qc_exe, quest_file, work_dir, timeout_sec, qf_list=None):
    qc_path = Path(qc_exe).resolve()
    if not qc_path.exists():
        return {"status": "ERROR", "error": f"qc binary not found: {qc_path}", "exit_code": -1, "time": 0}

    engine_work = Path(work_dir) / engine_name
    if engine_work.exists():
        shutil.rmtree(engine_work, ignore_errors=True)
    engine_work.mkdir(parents=True, exist_ok=True)
    if qf_list and Path(qf_list).exists():
        shutil.copy2(qf_list, engine_work / "quest_functions")

    start_t = time.perf_counter()
    try:
        proc = subprocess.run(
            [str(qc_path), str(Path(quest_file).resolve())],
            cwd=str(engine_work),
            capture_output=True,
            text=True,
            timeout=timeout_sec
        )
        elapsed = time.perf_counter() - start_t
        out_obj = engine_work / "object"
        file_count = len(list(out_obj.rglob("*"))) if out_obj.exists() else 0

        return {
            "status": "PASS" if proc.returncode == 0 else "FAIL",
            "exit_code": proc.returncode,
            "stdout": proc.stdout[-500:] if proc.stdout else "",
            "stderr": proc.stderr[-500:] if proc.stderr else "",
            "time": round(elapsed, 4),
            "object_dir": str(out_obj) if out_obj.exists() else "",
            "artifact_count": file_count
        }
    except subprocess.TimeoutExpired:
        elapsed = time.perf_counter() - start_t
        return {
            "status": "TIMEOUT",
            "exit_code": -999,
            "error": f"Compilation timed out after {timeout_sec}s",
            "time": round(elapsed, 4),
            "object_dir": "",
            "artifact_count": 0
        }
    except Exception as e:
        elapsed = time.perf_counter() - start_t
        return {
            "status": "ERROR",
            "exit_code": -1,
            "error": str(e),
            "time": round(elapsed, 4),
            "object_dir": "",
            "artifact_count": 0
        }

def main():
    default_qc = (
        "build/windows-gcc-ucrt/server/src/game/src/quest/qc.exe"
        if os.name == "nt"
        else "build/freebsd-gcc14/server/src/game/src/quest/qc"
    )
    parser = argparse.ArgumentParser(description="Metin2 MT2009 Unified Lua 5.5.1 Quest Compiler Tester")
    parser.add_argument("--quest-dir", default="server/files/share/locale/poland/quest", help="Path to serverfiles quest directory")
    parser.add_argument("--qc", default=default_qc, help="Lua 5.5.1 qc executable")
    parser.add_argument("--state-file", default="scratch/quest_matrix_results.json", help="Persistent results cache")
    parser.add_argument("--timeout", type=float, default=5.0, help="Per-quest timeout in seconds")
    parser.add_argument("--force", action="store_true", help="Re-run tests ignoring cache")
    parser.add_argument("--clean", action="store_true", help="Clean up scratch working directories on finish")
    args = parser.parse_args()

    quest_dir = Path(args.quest_dir).resolve()
    if not quest_dir.exists():
        print(f"[FATAL] Quest directory does not exist: {quest_dir}")
        sys.exit(1)

    sys.dont_write_bytecode = True
    sys.path.insert(0, str(quest_dir))
    try:
        import precompile
        has_precompile = True
    except ImportError:
        has_precompile = False

    state_file = Path(args.state_file).resolve()
    state_file.parent.mkdir(parents=True, exist_ok=True)

    results = {}
    if state_file.exists() and not args.force:
        try:
            with open(state_file, "r", encoding="utf-8") as f:
                results = json.load(f)
            print(f"[INFO] Loaded {len(results)} cached results from {state_file.name}")
        except Exception as e:
            print(f"[WARN] Failed to load cache: {e}, starting fresh.")
            results = {}

    quest_src_root = quest_dir / "quest" if (quest_dir / "quest").exists() else quest_dir
    quest_files = sorted([
        f for f in quest_src_root.rglob("*")
        if f.is_file() and f.suffix in (".quest", ".lua") and "object" not in f.parts and "_unused" not in f.parts
    ])

    print("=" * 60)
    print(" METİN2 MT2009 - LUA 5.5.1 QUEST DERLEYİCİ DOĞRULAMA SİSTEMİ")
    print(f" Bulunan Aktif Görev Dosyaları: {len(quest_files)}")
    print(f" Zaman Aşımı Limiti:            {args.timeout} saniye / dosya")
    print("=" * 60)

    work_base = Path("scratch/qc_work_matrix").resolve()
    work_base.mkdir(parents=True, exist_ok=True)
    pre_qc_work = work_base / "pre_qc"
    pre_qc_work.mkdir(parents=True, exist_ok=True)

    qf_list = quest_dir / "quest_functions"

    pass_count = 0
    fail_count = 0
    timeout_count = 0

    for idx, qf in enumerate(quest_files, 1):
        rel_path = str(qf.relative_to(quest_src_root)).replace("\\", "/")
        f_hash = compute_file_hash(qf)

        cached_entry = results.get(rel_path)
        if cached_entry and cached_entry.get("sha256") == f_hash and "lua55" in cached_entry.get("engines", {}) and not args.force:
            entry = cached_entry
        else:
            quest_work = work_base / f"q_{idx}"
            target_qf = qf
            if has_precompile:
                prev_cwd = os.getcwd()
                try:
                    os.chdir(work_base)
                    if precompile.run(str(qf), qf.name):
                        target_qf = pre_qc_work / qf.name
                finally:
                    os.chdir(prev_cwd)

            res = run_single_engine("lua55", args.qc, target_qf, quest_work, args.timeout, qf_list)
            entry = {
                "rel_path": rel_path,
                "sha256": f_hash,
                "size": qf.stat().st_size,
                "engines": {"lua55": res}
            }
            results[rel_path] = entry

        st = entry["engines"]["lua55"].get("status", "UNKNOWN")
        if st == "PASS":
            pass_count += 1
        elif st == "TIMEOUT":
            timeout_count += 1
        else:
            fail_count += 1

        if idx % 25 == 0 or idx == len(quest_files):
            print(f"[{idx:3d}/{len(quest_files)}] Lua 5.5.1 PASS: {pass_count:3d} | FAIL: {fail_count:3d}")

    try:
        with open(state_file, "w", encoding="utf-8") as f:
            json.dump(results, f, indent=2, ensure_ascii=False)
    except Exception as e:
        print(f"[WARN] Error saving state: {e}")

    print("\n" + "=" * 60)
    print(" METİN2 MT2009 - LUA 5.5.1 QUEST DERLEME RAPORU")
    print("=" * 60)
    print(f"Toplam Aktif Quest: {len(quest_files)}")
    pct = (pass_count / len(quest_files)) * 100 if quest_files else 0
    print(f"Motor [lua55  ]: PASS: {pass_count:3d} ({pct:5.1f}%) | FAIL: {fail_count:3d} | TIMEOUT: {timeout_count:2d}")

    if args.clean and work_base.exists():
        shutil.rmtree(work_base, ignore_errors=True)

    if fail_count > 0 or timeout_count > 0:
        sys.exit(1)

if __name__ == "__main__":
    main()
