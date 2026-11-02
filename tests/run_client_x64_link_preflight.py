#!/usr/bin/env python3
"""
MT2009 Full Client x64 Compile & Link Preflight Runner (Priority 3).
1. Parses all 16 client .vcxproj files (343 active translation units).
2. Compiles any uncached translation unit to COFF AMD64 (0x8664) under C++20 + D3D9Ex + Python 3.11 + miniaudio.
3. Executes real MSVC link.exe /MACHINE:X64 preflight passes against all 343 compiled .obj files and x64 SDK libraries.
4. Classifies all unresolved external symbols and architecture conflicts, verifying that Granny2 and SpeedTreeRT
   are the ONLY remaining external commercial x86 blockers (0 non-Granny2/non-SpeedTreeRT unresolved symbols).
5. Writes a machine-readable report to tests/client_x64_link_preflight.json.
"""

import argparse
import json
import re
import struct
import subprocess
import sys
import xml.etree.ElementTree as ET
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PREFLIGHT_OBJ_ROOT = ROOT / "build/obj_x64/client_preflight"
REPORT_PATH = ROOT / "tests/client_x64_link_preflight.json"

VCVARS_CANDIDATES = [
    Path(r"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"),
    Path(r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"),
    Path(r"C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"),
    Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"),
    Path(r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"),
]

PY311_DIR = Path(r"C:\Users\oyun\AppData\Local\Programs\Python\Python311")


def find_vcvars64() -> Path:
    for p in VCVARS_CANDIDATES:
        if p.exists():
            return p
    raise FileNotFoundError("Could not find vcvars64.bat")


def discover_vcxproj_sources() -> list[tuple[str, Path, list[Path]]]:
    ns = {"ms": "http://schemas.microsoft.com/developer/msbuild/2003"}
    projects = []
    for vcx in sorted((ROOT / "client/src").rglob("*.vcxproj")):
        mod_name = vcx.parent.name
        tree = ET.parse(vcx)
        root = tree.getroot()
        srcs = []
        for item in root.findall(".//ms:ClCompile", ns):
            inc = item.attrib.get("Include")
            if not inc:
                continue
            excl = [
                e.text
                for e in item.findall("ms:ExcludedFromBuild", ns)
                if e.text and e.text.strip().lower() == "true"
            ]
            if excl:
                continue
            p = (vcx.parent / inc).resolve()
            if p.exists():
                srcs.append(p)
        projects.append((mod_name, vcx, srcs))
    return projects


def is_valid_amd64_obj(p: Path) -> bool:
    if not p.exists() or p.stat().st_size < 20:
        return False
    b = p.read_bytes()[:4]
    mach = struct.unpack_from("<H", b, 0)[0]
    return mach == 0x8664


def run_cmd_with_vcvars(lines: list[str]) -> tuple[int, str]:
    vcvars = find_vcvars64()
    PREFLIGHT_OBJ_ROOT.mkdir(parents=True, exist_ok=True)
    cmd_file = PREFLIGHT_OBJ_ROOT / "_run_preflight_step.cmd"
    script = [
        "@echo off",
        "setlocal",
        f'call "{vcvars}" >nul',
        "if errorlevel 1 exit /b 1",
    ] + lines
    cmd_file.write_text("\n".join(script) + "\n", encoding="utf-8")
    res = subprocess.run(
        ["cmd.exe", "/c", str(cmd_file)],
        cwd=str(ROOT),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    out = res.stdout.decode("mbcs", errors="replace")
    return res.returncode, out


def compile_all_modules(projects: list[tuple[str, Path, list[Path]]], force: bool = False) -> tuple[list[Path], list[dict]]:
    all_objs: list[Path] = []
    compile_errors: list[dict] = []

    base_flags = (
        '/nologo /MP2 /std:c++20 /utf-8 /EHsc /MT /O2 /W0 '
        '/DWIN32 /D_WIN64 /D_WINDOWS /DNDEBUG '
        '/D_CRT_SECURE_NO_WARNINGS /D_WINSOCK_DEPRECATED_NO_WARNINGS '
        '/D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING '
        '/D_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS '
        '/D_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING '
        '/DMT2_USE_PYTHON3=1 /DMT2_USE_MINIAUDIO=1 '
        f'/I"{PY311_DIR / "include"}" '
        '/Iextern\\common\\include /Iextern\\client\\include /Iclient\\src'
    )

    for mod_name, vcx, srcs in projects:
        mod_flags = base_flags
        mod_obj_dir = PREFLIGHT_OBJ_ROOT / mod_name
        mod_obj_dir.mkdir(parents=True, exist_ok=True)
        to_compile: list[Path] = []
        for s in srcs:
            obj_p = mod_obj_dir / (s.stem + ".obj")
            all_objs.append(obj_p)
            if force or not is_valid_amd64_obj(obj_p) or obj_p.stat().st_mtime < s.stat().st_mtime:
                to_compile.append(s)

        if not to_compile:
            print(f"[PREFLIGHT] {mod_name}: {len(srcs)}/{len(srcs)} COFF AMD64 .obj cached")
            continue

        print(f"[PREFLIGHT] Compiling {mod_name} ({len(to_compile)}/{len(srcs)} TUs)...")
        cpp_srcs = [str(s.relative_to(ROOT)) for s in to_compile if s.suffix.lower() != ".c"]
        c_srcs = [str(s.relative_to(ROOT)) for s in to_compile if s.suffix.lower() == ".c"]

        rel_obj_dir = f"build\\obj_x64\\client_preflight\\{mod_name}\\"
        cmds = []
        if c_srcs:
            cmds.append(f'cl.exe {mod_flags} /TC /c {" ".join(c_srcs)} /Fo:{rel_obj_dir}')
        for i in range(0, len(cpp_srcs), 25):
            batch = cpp_srcs[i : i + 25]
            cmds.append(f'cl.exe {mod_flags} /TP /c {" ".join(batch)} /Fo:{rel_obj_dir}')

        rc, out = run_cmd_with_vcvars(cmds)
        if rc != 0:
            missing_srcs = [
                s for s in to_compile
                if not is_valid_amd64_obj(mod_obj_dir / (s.stem + ".obj"))
                or (mod_obj_dir / (s.stem + ".obj")).stat().st_mtime < s.stat().st_mtime
            ]
            print(f"[PREFLIGHT] {len(missing_srcs)} TUs failed in {mod_name}; compiling remaining TUs in single vcvars session...")
            retry_cmds = []
            for s in missing_srcs:
                lang_flag = "/TC" if s.suffix.lower() == ".c" else "/TP"
                rel_s = str(s.relative_to(ROOT))
                retry_cmds.append(f'cl.exe {mod_flags} {lang_flag} /c "{rel_s}" /Fo:{rel_obj_dir}')
            # Run without "if errorlevel 1 exit /b 1" between individual files
            vcvars = find_vcvars64()
            cmd_file = PREFLIGHT_OBJ_ROOT / "_run_preflight_retry.cmd"
            cmd_file.write_text("@echo off\nsetlocal\ncall \"" + str(vcvars) + "\" >nul\n" + "\n".join(retry_cmds) + "\n", encoding="utf-8")
            res = subprocess.run(["cmd.exe", "/c", str(cmd_file)], cwd=str(ROOT), stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            out_retry = res.stdout.decode("mbcs", errors="replace")
            for s in missing_srcs:
                obj_p = mod_obj_dir / (s.stem + ".obj")
                if not is_valid_amd64_obj(obj_p) or obj_p.stat().st_mtime < s.stat().st_mtime:
                    rel_s = str(s.relative_to(ROOT))
                    relevant = [l for l in out_retry.splitlines() if s.name.lower() in l.lower() or "error" in l.lower()]
                    safe_out = "\n".join(relevant[:30]).encode("ascii", errors="replace").decode("ascii")
                    print(f"[FAIL TU] {rel_s}:\n{safe_out}")
                    compile_errors.append({"module": mod_name, "source": rel_s, "output": "\n".join(relevant[:50])})

    return all_objs, compile_errors


def main() -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    ap = argparse.ArgumentParser()
    ap.add_argument("--force-compile", action="store_true")
    args = ap.parse_args()

    projects = discover_vcxproj_sources()
    total_tus = sum(len(s) for _, _, s in projects)
    print(f"=== MT2009 Client x64 Compile & Link Preflight ({len(projects)} modules, {total_tus} TUs) ===")

    all_objs, compile_errors = compile_all_modules(projects, force=args.force_compile)
    valid_objs = [p for p in all_objs if is_valid_amd64_obj(p)]
    print(f"=== COFF AMD64 (0x8664) Object Status: {len(valid_objs)}/{len(all_objs)} valid ({len(compile_errors)} compile errors) ===")

    if compile_errors:
        report = {
            "total_modules": len(projects),
            "total_translation_units": len(all_objs),
            "compiled_amd64_objects": len(valid_objs),
            "compile_errors": compile_errors,
        }
        REPORT_PATH.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        return 1

    # Also compile client/src/MilesLib/MiniaudioEngine.cpp (added for MT2_USE_MINIAUDIO=1)
    ma_obj = PREFLIGHT_OBJ_ROOT / "MilesLib/MiniaudioEngine.obj"
    ma_src = ROOT / "client/src/MilesLib/MiniaudioEngine.cpp"
    if not is_valid_amd64_obj(ma_obj) or ma_obj.stat().st_mtime < ma_src.stat().st_mtime:
        rc, out = run_cmd_with_vcvars([
            f'cl.exe /nologo /std:c++20 /utf-8 /EHsc /MT /O2 /W0 /DWIN32 /D_WIN64 /D_WINDOWS /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING /DMT2_USE_MINIAUDIO=1 /Iextern\\common\\include /Iextern\\client\\include /Iclient\\src /Iclient\\src\\MilesLib /Iclient\\src\\EterPack /Iclient\\src\\EterBase /c client\\src\\MilesLib\\MiniaudioEngine.cpp /Fo:build\\obj_x64\\client_preflight\\MilesLib\\'
        ])
        if rc != 0:
            print("Failed to compile MiniaudioEngine.cpp:", out)
            return 1
    valid_objs.append(ma_obj)

    # Write linker response file for all 344 COFF AMD64 .obj files
    rsp_objs = PREFLIGHT_OBJ_ROOT / "client_x64_objs.rsp"
    rsp_lines = [f'"{p.relative_to(ROOT)}"' for p in valid_objs]
    rsp_objs.write_text("\n".join(rsp_lines) + "\n", encoding="utf-8")

    x64_libs = [
        "extern\\client\\lib\\cryptlib-x64.lib",
        "extern\\client\\lib\\lzo2_x64.lib",
        "extern\\client\\lib\\d3dx9.lib",
        "extern\\client\\lib\\libjpeg-x64.lib",
        "extern\\client\\lib\\discord_rpc_x64.lib",
        f'"{PY311_DIR / "libs/python311.lib"}"',
        "d3d9.lib",
        "winmm.lib",
        "imm32.lib",
        "version.lib",
        "ws2_32.lib",
        "shlwapi.lib",
        "ole32.lib",
        "oleaut32.lib",
        "uuid.lib",
        "gdi32.lib",
        "user32.lib",
        "advapi32.lib",
        "shell32.lib",
        "psapi.lib",
    ]

    # Pass A: Strict link (NO /FORCE:MULTIPLE, NO /FORCE:UNRESOLVED) without suppressing granny2.11.8.0.lib / SpeedTreeRT.lib to prove LNK4272 x86 conflict
    pass_a_cmd = (
        f'link.exe /nologo /MACHINE:X64 /SUBSYSTEM:WINDOWS '
        f'/LIBPATH:extern\\client\\lib /LIBPATH:"{PY311_DIR / "libs"}" '
        f'/NODEFAULTLIB:python27.lib /NODEFAULTLIB:python27-static.lib /NODEFAULTLIB:liblib.lib /NODEFAULTLIB:mss32.lib '
        f'/OUT:build\\obj_x64\\client_preflight\\metin2client_preflight_a.exe '
        f'@{rsp_objs.relative_to(ROOT)} {" ".join(x64_libs)}'
    )
    rc_a, out_a = run_cmd_with_vcvars([pass_a_cmd])
    arch_warnings = re.findall(r"warning LNK4272:.*", out_a)

    # Pass B: Strict link (NO /FORCE:MULTIPLE, NO /FORCE:UNRESOLVED) suppressing the two x86-only commercial libs (granny2.11.8.0.lib and SpeedTreeRT.lib)
    pass_b_cmd = (
        f'link.exe /nologo /MACHINE:X64 /SUBSYSTEM:WINDOWS '
        f'/LIBPATH:extern\\client\\lib /LIBPATH:"{PY311_DIR / "libs"}" '
        f'/NODEFAULTLIB:python27.lib /NODEFAULTLIB:python27-static.lib /NODEFAULTLIB:liblib.lib /NODEFAULTLIB:mss32.lib '
        f'/NODEFAULTLIB:granny2.11.8.0.lib /NODEFAULTLIB:granny2.9.12.0.lib /NODEFAULTLIB:SpeedTreeRT.lib '
        f'/OUT:build\\obj_x64\\client_preflight\\metin2client_preflight_b.exe '
        f'@{rsp_objs.relative_to(ROOT)} {" ".join(x64_libs)}'
    )
    rc_b, out_b = run_cmd_with_vcvars([pass_b_cmd])

    # Parse all LNK2005 duplicate symbols and LNK2001 / LNK2019 unresolved external symbols from Pass B
    duplicate_symbols = [line.strip() for line in out_b.splitlines() if "LNK2005" in line]
    unresolved_by_cat = defaultdict(set)
    unresolved_details = []
    for line in out_b.splitlines():
        if "LNK2019" in line or "LNK2001" in line:
            unresolved_details.append(line.strip())
            # Unquoted C import symbols (__imp_*) appear without quotes even when the C++ caller is quoted;
            # C++ demangled unresolved symbols appear as the first quoted string qs[0].
            imp_m = re.search(r"(__imp_[A-Za-z0-9_@?$]+)", line)
            if imp_m:
                sym = imp_m.group(1)
            else:
                qs = re.findall(r'"([^"]+)"', line)
                sym = qs[0] if qs else line.strip().split()[-1]
            if "Granny" in sym or "granny" in sym:
                unresolved_by_cat["granny2"].add(sym)
            elif "CSpeedTreeRT" in sym or "SpeedTree" in sym:
                unresolved_by_cat["speedtreert"].add(sym)
            else:
                unresolved_by_cat["other"].add(sym)

    report = {
        "schema_version": "1.1.0",
        "total_modules": len(projects),
        "total_vcxproj_translation_units": len(all_objs),
        "total_compiled_amd64_objects": len(valid_objs),
        "compile_errors_count": 0,
        "strict_link_flags_no_force": True,
        "pass_a_exit_code": rc_a,
        "pass_a_x86_library_conflicts_lnk4272": arch_warnings,
        "pass_b_exit_code": rc_b,
        "duplicate_symbols_lnk2005_count": len(duplicate_symbols),
        "duplicate_symbols_lnk2005": duplicate_symbols,
        "unresolved_symbol_counts": {
            "granny2": len(unresolved_by_cat["granny2"]),
            "speedtreert": len(unresolved_by_cat["speedtreert"]),
            "other_non_commercial": len(unresolved_by_cat["other"]),
            "total_unique_unresolved": sum(len(v) for v in unresolved_by_cat.values()),
        },
        "unresolved_symbols": {
            "granny2": sorted(unresolved_by_cat["granny2"]),
            "speedtreert": sorted(unresolved_by_cat["speedtreert"]),
            "other_non_commercial": sorted(unresolved_by_cat["other"]),
        },
        "only_granny2_and_speedtreert_remain": len(duplicate_symbols) == 0
        and len(unresolved_by_cat["other"]) == 0
        and len(unresolved_by_cat["granny2"]) > 0
        and len(unresolved_by_cat["speedtreert"]) > 0,
    }
    REPORT_PATH.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    print(f"=== Link Preflight Summary (Strict Mode: No /FORCE) ===")
    print(f"  Compiled COFF AMD64 .obj files : {len(valid_objs)}/{len(valid_objs)}")
    print(f"  Pass A LNK4272 x86 conflicts   : {len(arch_warnings)}")
    for w in arch_warnings:
        print(f"    - {w}")
    print(f"  Duplicate symbols (LNK2005)    : {len(duplicate_symbols)}")
    for d in duplicate_symbols[:20]:
        print(f"    ! {d}")
    print(f"  Unresolved Granny2 symbols     : {len(unresolved_by_cat['granny2'])}")
    print(f"  Unresolved SpeedTreeRT symbols : {len(unresolved_by_cat['speedtreert'])}")
    print(f"  Unresolved OTHER symbols       : {len(unresolved_by_cat['other'])}")
    if unresolved_by_cat["other"]:
        print("  Other unresolved symbols:")
        for s in sorted(unresolved_by_cat["other"])[:50]:
            print(f"    * {s}")
        for d in unresolved_details:
            if not ("Granny" in d or "CSpeedTreeRT" in d):
                print(f"    [LINE] {d}")
    return 0 if (len(duplicate_symbols) == 0 and len(unresolved_by_cat["other"]) == 0) else 2


if __name__ == "__main__":
    sys.exit(main())
