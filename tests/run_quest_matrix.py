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

def run_single_engine(engine_name, qc_exe, quest_file, work_dir, timeout_sec):
    qc_path = Path(qc_exe).resolve()
    if not qc_path.exists():
        return {"status": "ERROR", "error": f"qc binary not found: {qc_path}", "exit_code": -1, "time": 0}

    engine_work = Path(work_dir) / engine_name
    if engine_work.exists():
        shutil.rmtree(engine_work, ignore_errors=True)
    engine_work.mkdir(parents=True, exist_ok=True)

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
    parser = argparse.ArgumentParser(description="Metin2 MT2009 Unified Quest Matrix Tester")
    parser.add_argument("--quest-dir", default="scratch/ftp_quests/usr/home/mt2/share/locale/poland/quest", help="Path to extracted quests")
    parser.add_argument("--qc50", default="build/windows-gcc-ucrt/server/src/game/src/quest/qc.exe", help="Lua 5.0 qc executable")
    parser.add_argument("--qc52", default="build/windows-gcc-ucrt/server/src/game/src/quest/qc_lua52.exe", help="Lua 5.2 qc executable")
    parser.add_argument("--qc-modern", default="build/windows-gcc-ucrt/server/src/game/src/quest/qc_lua55.exe", help="Modern Lua qc executable (optional)")
    parser.add_argument("--state-file", default="scratch/quest_matrix_results.json", help="Persistent results cache")
    parser.add_argument("--timeout", type=float, default=5.0, help="Per-quest timeout in seconds")
    parser.add_argument("--force", action="store_true", help="Re-run tests ignoring cache")
    parser.add_argument("--clean", action="store_true", help="Clean up scratch working directories on finish")
    args = parser.parse_args()

    quest_dir = Path(args.quest_dir).resolve()
    if not quest_dir.exists():
        print(f"[FATAL] Quest directory does not exist: {quest_dir}")
        sys.exit(1)

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

    # Envanter: Tüm .quest ve .lua dosyaları (object dizini hariç)
    quest_files = sorted([
        f for f in quest_dir.rglob("*.quest")
        if "object" not in f.parts
    ])
    lua_files = sorted([
        f for f in quest_dir.rglob("*.lua")
        if "object" not in f.parts
    ])

    print("=" * 60)
    print(" METİN2 MT2009 - TEK MERKEZÎ QUEST TEST MATRİSİ")
    print(f" Bulunan .quest Dosyaları: {len(quest_files)}")
    print(f" Bulunan .lua Dosyaları:   {len(lua_files)}")
    print(f" Zaman Aşımı Limiti:       {args.timeout} saniye / dosya")
    print("=" * 60)

    engines = [("lua50", args.qc50), ("lua52", args.qc52)]
    if args.qc_modern and Path(args.qc_modern).exists():
        engines.append(("lua55", args.qc_modern))

    work_base = Path("scratch/qc_work_matrix").resolve()
    work_base.mkdir(parents=True, exist_ok=True)

    pass_counts = {eng[0]: 0 for eng in engines}
    fail_counts = {eng[0]: 0 for eng in engines}
    timeout_counts = {eng[0]: 0 for eng in engines}
    both_pass_52 = 0
    identical_outputs_52 = 0
    both_pass_55 = 0
    identical_outputs_55 = 0

    for idx, qf in enumerate(quest_files, 1):
        rel_path = str(qf.relative_to(quest_dir)).replace("\\", "/")
        f_hash = compute_file_hash(qf)

        cached_entry = results.get(rel_path)
        if cached_entry and cached_entry.get("sha256") == f_hash and not args.force:
            entry = cached_entry
        else:
            entry = {
                "rel_path": rel_path,
                "sha256": f_hash,
                "size": qf.stat().st_size,
                "engines": {},
                "parity": {}
            }

        missing_engines = [eng for eng in engines if eng[0] not in entry.get("engines", {})]
        if missing_engines:
            quest_work = work_base / f"q_{idx}"
            for eng_name, eng_bin in missing_engines:
                res = run_single_engine(eng_name, eng_bin, qf, quest_work, args.timeout)
                entry["engines"][eng_name] = res

            # Parite Karşılaştırması 5.0 vs 5.2
            if entry["engines"].get("lua50", {}).get("status") == "PASS" and \
               entry["engines"].get("lua52", {}).get("status") == "PASS":
                dir50 = entry["engines"]["lua50"]["object_dir"]
                dir52 = entry["engines"]["lua52"]["object_dir"]
                is_ident, reason, file_count = compare_directories(dir50, dir52)
                entry["parity"]["lua50_vs_lua52"] = {
                    "identical": is_ident,
                    "reason": reason,
                    "file_count": file_count
                }

            # Parite Karşılaştırması 5.0 vs 5.5
            if entry["engines"].get("lua50", {}).get("status") == "PASS" and \
               entry["engines"].get("lua55", {}).get("status") == "PASS":
                dir50 = entry["engines"]["lua50"]["object_dir"]
                dir55 = entry["engines"]["lua55"]["object_dir"]
                is_ident, reason, file_count = compare_directories(dir50, dir55)
                entry["parity"]["lua50_vs_lua55"] = {
                    "identical": is_ident,
                    "reason": reason,
                    "file_count": file_count
                }

            # Her quest sonrası durumu kaydet (kesintiye dayanıklı)
            results[rel_path] = entry
            try:
                temp_file = state_file.with_suffix(".tmp")
                with open(temp_file, "w", encoding="utf-8") as f:
                    json.dump(results, f, indent=2, ensure_ascii=False)
                temp_file.replace(state_file)
            except Exception as e:
                print(f"[WARN] Error saving state: {e}")

        # İstatistikleri güncelle
        for eng_name, _ in engines:
            st = entry.get("engines", {}).get(eng_name, {}).get("status", "UNKNOWN")
            if st == "PASS":
                pass_counts[eng_name] += 1
            elif st == "TIMEOUT":
                timeout_counts[eng_name] += 1
            else:
                fail_counts[eng_name] += 1

        p_info_52 = entry.get("parity", {}).get("lua50_vs_lua52", {})
        if entry.get("engines", {}).get("lua50", {}).get("status") == "PASS" and \
           entry.get("engines", {}).get("lua52", {}).get("status") == "PASS":
            both_pass_52 += 1
            if p_info_52.get("identical", False):
                identical_outputs_52 += 1

        if "lua55" in pass_counts:
            p_info_55 = entry.get("parity", {}).get("lua50_vs_lua55", {})
            if entry.get("engines", {}).get("lua50", {}).get("status") == "PASS" and \
               entry.get("engines", {}).get("lua55", {}).get("status") == "PASS":
                both_pass_55 += 1
                if p_info_55.get("identical", False):
                    identical_outputs_55 += 1

        if idx % 25 == 0 or idx == len(quest_files):
            stat_str = f"[{idx:3d}/{len(quest_files)}] 5.0: {pass_counts['lua50']:3d} | 5.2: {pass_counts['lua52']:3d}"
            if "lua55" in pass_counts:
                stat_str += f" | 5.5: {pass_counts['lua55']:3d}"
            print(stat_str)

    # Raporlama
    print("\n" + "=" * 60)
    print(" METİN2 MT2009 - KONSOLİDE QUEST TEST RAPORU")
    print("=" * 60)
    print(f"Toplam Test Edilen Quest: {len(quest_files)}")
    for eng_name, _ in engines:
        p = pass_counts[eng_name]
        f = fail_counts[eng_name]
        t = timeout_counts[eng_name]
        pct = (p / len(quest_files)) * 100 if quest_files else 0
        print(f"Motor [{eng_name:7s}]: PASS: {p:3d} ({pct:5.1f}%) | FAIL: {f:3d} | TIMEOUT: {t:2d}")

    print(f"5.0 ve 5.2 Ortak Başarılı: {both_pass_52}")
    parity_pct_52 = (identical_outputs_52 / both_pass_52 * 100) if both_pass_52 else 0
    print(f"5.0 vs 5.2 Bayt-Bayt %100 Birebir Eşdeğer: {identical_outputs_52} / {both_pass_52} ({parity_pct_52:5.1f}%)")

    if "lua55" in pass_counts:
        print(f"5.0 ve 5.5 Ortak Başarılı: {both_pass_55}")
        parity_pct_55 = (identical_outputs_55 / both_pass_55 * 100) if both_pass_55 else 0
        print(f"5.0 vs 5.5 Bayt-Bayt %100 Birebir Eşdeğer: {identical_outputs_55} / {both_pass_55} ({parity_pct_55:5.1f}%)")

    # Fark veya hata analizi dökümü
    diffs = [
        (k, v) for k, v in results.items()
        if len(set(e.get("status") for e in v.get("engines", {}).values())) > 1
    ]
    if diffs:
        print(f"\n[DİKKAT] Motorlar Arası Durum Farkı Olan Dosyalar ({len(diffs)} adet):")
        for name, entry in diffs:
            stat_summary = ", ".join(f"{eng}={entry['engines'][eng]['status']}" for eng in entry.get("engines", {}))
            print(f"  - {name}: {stat_summary}")

    if args.clean and work_base.exists():
        shutil.rmtree(work_base, ignore_errors=True)

if __name__ == "__main__":
    main()
