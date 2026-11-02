#!/usr/bin/env python3
"""
Deterministic, reproducible builder and verifier for MT2009 Windows x64 (COFF AMD64)
client static/import libraries and runtime DLLs from committed repository sources.

Replaces ad-hoc scratch scripts and records exact compiler/linker flags, CRT/ABI
options (/MT, C++20, CRYPTOPP_DISABLE_ASM), and PE import-table hygiene checks.
"""

import argparse
import hashlib
import os
import re
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

VCVARS_CANDIDATES = [
    Path(r"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"),
    Path(r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"),
    Path(r"C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"),
    Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"),
    Path(r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"),
]


def find_vcvars64() -> Path:
    for p in VCVARS_CANDIDATES:
        if p.exists():
            return p
    raise FileNotFoundError("Could not find vcvars64.bat in standard Visual Studio locations.")


def run_With_vcvars(commands: list[str]) -> None:
    vcvars = find_vcvars64()
    cmd_dir = ROOT / "build/obj_x64/sdk_build"
    cmd_dir.mkdir(parents=True, exist_ok=True)
    cmd_file = cmd_dir / "_run_sdk_step.cmd"
    lines = [
        "@echo off",
        "setlocal",
        f'call "{vcvars}" >nul',
        "if errorlevel 1 exit /b 1",
    ]
    for c in commands:
        lines.append(c)
        lines.append("if errorlevel 1 exit /b 1")
    cmd_file.write_text("\n".join(lines) + "\n", encoding="utf-8")
    res = subprocess.run(["cmd.exe", "/c", str(cmd_file)], cwd=str(ROOT))
    if res.returncode != 0:
        raise RuntimeError(f"Command batch failed with exit code {res.returncode}")


def parse_pe_imports(pe_path: Path) -> list[str]:
    b = pe_path.read_bytes()
    pe_off = struct.unpack_from("<I", b, 0x3C)[0]
    num_sec = struct.unpack_from("<H", b, pe_off + 6)[0]
    opt_sz = struct.unpack_from("<H", b, pe_off + 20)[0]
    magic = struct.unpack_from("<H", b, pe_off + 24)[0]
    dd_off = pe_off + 24 + (112 if magic == 0x20B else 96)
    imp_rva, _ = struct.unpack_from("<II", b, dd_off + 8)
    if imp_rva == 0:
        return []
    sec_off = pe_off + 24 + opt_sz
    secs = []
    for i in range(num_sec):
        s = b[sec_off + i * 40 : sec_off + (i + 1) * 40]
        vsz, va, rsz, ptr = struct.unpack_from("<IIII", s, 8)
        secs.append((va, max(vsz, rsz), ptr))

    def rva2off(rva: int):
        for va, sz, ptr in secs:
            if va <= rva < va + sz:
                return ptr + (rva - va)
        return None

    off = rva2off(imp_rva)
    imports = []
    while off is not None and off + 20 <= len(b):
        ilt, ts, fc, name_rva, iat = struct.unpack_from("<IIIII", b, off)
        if ilt == 0 and name_rva == 0 and iat == 0:
            break
        noff = rva2off(name_rva)
        if noff is not None:
            end = b.find(b"\x00", noff)
            imports.append(b[noff:end].decode("ascii", errors="ignore"))
        off += 20
    return imports


def inspect_coff_archive_machines(lib_path: Path) -> set[int]:
    b = lib_path.read_bytes()
    if not b.startswith(b"!<arch>\n"):
        return set()
    pos = 8
    n = len(b)
    machines = set()
    while pos + 60 <= n:
        hdr = b[pos : pos + 60]
        name = hdr[:16].decode("ascii", errors="ignore").strip()
        try:
            sz = int(hdr[48:58].decode("ascii", errors="ignore").strip())
        except ValueError:
            break
        data_pos = pos + 60
        if name not in ("/", "//") and sz >= 20:
            sig1, sig2, ver, mach = struct.unpack_from("<HHHH", b, data_pos)
            if sig1 == 0x0000 and sig2 == 0xFFFF:
                machines.add(mach)
            elif sig1 in (0x014C, 0x8664):
                machines.add(sig1)
        pos = data_pos + sz + (sz & 1)
    return machines


def build_lzo_x64() -> None:
    print("[SDK] Building lzo2_x64.lib (LZO 2.10 MSVC AMD64 /MT)...")
    obj_dir = ROOT / "build/obj_x64/sdk_build/lzo"
    obj_dir.mkdir(parents=True, exist_ok=True)
    run_With_vcvars([
        "cl.exe /nologo /TC /MT /O2 /W3 /Iextern\\client\\include /Iserver\\src\\game\\src /c server\\src\\game\\src\\minilzo.c /Fo:build\\obj_x64\\sdk_build\\lzo\\minilzo.obj",
        "lib.exe /nologo /MACHINE:X64 /OUT:extern\\client\\lib\\lzo2_x64.lib build\\obj_x64\\sdk_build\\lzo\\minilzo.obj",
    ])


def build_libjpeg_x64() -> None:
    print("[SDK] Building libjpeg-x64.lib (IJG libjpeg 9.3 MSVC AMD64 /MT)...")
    obj_dir = ROOT / "build/obj_x64/sdk_build/libjpeg"
    obj_dir.mkdir(parents=True, exist_ok=True)
    c_files = sorted(str(f.relative_to(ROOT)) for f in (ROOT / "extern/client/libjpeg").glob("*.c"))
    if not c_files:
        raise RuntimeError("No .c files found in extern/client/libjpeg")
    run_With_vcvars([
        f"cl.exe /nologo /MP2 /TC /MT /O2 /W0 /D_CRT_SECURE_NO_WARNINGS /Iextern\\client\\include\\libjpeg /Iextern\\client\\libjpeg /c {' '.join(c_files)} /Fo:build\\obj_x64\\sdk_build\\libjpeg\\",
        "lib.exe /nologo /MACHINE:X64 /OUT:extern\\client\\lib\\libjpeg-x64.lib build\\obj_x64\\sdk_build\\libjpeg\\*.obj",
    ])


def build_discord_rpc_x64() -> None:
    print("[SDK] Building discord_rpc_x64.lib (discord-rpc 3.4.0 MSVC AMD64 /MT)...")
    obj_dir = ROOT / "build/obj_x64/sdk_build/discord_rpc"
    obj_dir.mkdir(parents=True, exist_ok=True)
    cpp_files = sorted(str(f.relative_to(ROOT)) for f in (ROOT / "extern/client/discord-rpc").glob("*.cpp"))
    if not cpp_files:
        raise RuntimeError("No .cpp files found in extern/client/discord-rpc")
    run_With_vcvars([
        f"cl.exe /nologo /MP2 /std:c++20 /EHsc /MT /O2 /W3 /DDISCORD_WINDOWS /D_CRT_SECURE_NO_WARNINGS /Iextern\\client\\include /Iextern\\common\\include /Iextern\\client\\discord-rpc /c {' '.join(cpp_files)} /Fo:build\\obj_x64\\sdk_build\\discord_rpc\\",
        "lib.exe /nologo /MACHINE:X64 /OUT:extern\\client\\lib\\discord_rpc_x64.lib build\\obj_x64\\sdk_build\\discord_rpc\\*.obj",
    ])


def build_cryptopp_x64() -> None:
    print("[SDK] Building cryptlib-x64.lib (Crypto++ 8.9.0 MSVC AMD64 /MT /DCRYPTOPP_DISABLE_ASM)...")
    obj_dir = ROOT / "build/obj_x64/cryptopp890"
    obj_dir.mkdir(parents=True, exist_ok=True)
    srcs = sorted([
        str(f.relative_to(ROOT))
        for f in (ROOT / "extern/server/cryptopp").glob("*.cpp")
        if not re.match(
            r"^(adhoc|test|bench\d*|datatest|dlltest|fipsalgt|validat\d*|regtest\d*|pch|arm_.*|ppc_.*|neon_.*|sse_.*|.*_sse|.*_avx|.*_ppc|.*_simd)\.cpp$",
            f.name,
        )
    ])
    cmds = []
    compat_hdr = ROOT / "extern/server/cryptopp/msvc_stdext_compat.h"
    for i in range(0, len(srcs), 50):
        batch = " ".join(srcs[i : i + 50])
        cmds.append(
            f'cl.exe /nologo /MP2 /std:c++20 /EHsc /MT /O2 /W0 /FI"{compat_hdr}" /D_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING /DCRYPTOPP_DISABLE_ASM /DNDEBUG /Iextern\\server\\include /Iextern\\server\\cryptopp /c {batch} /Fo:build\\obj_x64\\cryptopp890\\'
        )
    cmds.append("lib.exe /nologo /MACHINE:X64 /OUT:extern\\client\\lib\\cryptlib-x64.lib build\\obj_x64\\cryptopp890\\*.obj")
    run_With_vcvars(cmds)


def generate_def_from_pe_dll(dll_path: Path, def_path: Path) -> None:
    b = dll_path.read_bytes()
    pe_off = struct.unpack_from("<I", b, 0x3C)[0]
    num_sec = struct.unpack_from("<H", b, pe_off + 6)[0]
    opt_sz = struct.unpack_from("<H", b, pe_off + 20)[0]
    magic = struct.unpack_from("<H", b, pe_off + 24)[0]
    dd_off = pe_off + 24 + (112 if magic == 0x20B else 96)
    exp_rva, exp_sz = struct.unpack_from("<II", b, dd_off)
    sec_off = pe_off + 24 + opt_sz
    secs = []
    for i in range(num_sec):
        s = b[sec_off + i * 40 : sec_off + (i + 1) * 40]
        vsz, va, rsz, ptr = struct.unpack_from("<IIII", s, 8)
        chars = struct.unpack_from("<I", s, 36)[0]
        secs.append((va, max(vsz, rsz), ptr, chars))

    def rva2off_and_chars(rva: int):
        for va, sz, ptr, chars in secs:
            if va <= rva < va + sz:
                return ptr + (rva - va), chars
        return None, 0

    eoff, _ = rva2off_and_chars(exp_rva)
    if eoff is None:
        raise RuntimeError(f"No export table in {dll_path}")
    num_funcs, num_names, addr_funcs, addr_names, addr_ords = struct.unpack_from(
        "<IIIII", b, eoff + 20
    )
    foff, _ = rva2off_and_chars(addr_funcs)
    noff, _ = rva2off_and_chars(addr_names)
    ooff, _ = rva2off_and_chars(addr_ords)
    lines = [f'LIBRARY "{dll_path.name}"', "EXPORTS"]
    for i in range(num_names):
        name_rva = struct.unpack_from("<I", b, noff + i * 4)[0]
        ord_idx = struct.unpack_from("<H", b, ooff + i * 2)[0]
        func_rva = struct.unpack_from("<I", b, foff + ord_idx * 4)[0]
        s_off, _ = rva2off_and_chars(name_rva)
        end = b.find(b"\x00", s_off)
        sym = b[s_off:end].decode("ascii")
        if exp_rva <= func_rva < exp_rva + exp_sz:
            continue
        _, sym_chars = rva2off_and_chars(func_rva)
        is_code = bool(sym_chars & 0x20000000)
        lines.append(f"    {sym}" if is_code else f"    {sym} DATA")
    def_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def build_openssl_x64() -> None:
    print("[SDK] Building libssl_x64.lib & libcrypto_x64.lib (OpenSSL 3.6.5 MSVC AMD64 import libs)...")
    obj_dir = ROOT / "build/obj_x64/sdk_build/openssl"
    obj_dir.mkdir(parents=True, exist_ok=True)
    ssl_def = obj_dir / "libssl-3-x64.def"
    crypto_def = obj_dir / "libcrypto-3-x64.def"
    generate_def_from_pe_dll(ROOT / "extern/client/dll/libssl-3-x64.dll", ssl_def)
    generate_def_from_pe_dll(ROOT / "extern/client/dll/libcrypto-3-x64.dll", crypto_def)
    run_With_vcvars([
        f'lib.exe /nologo /MACHINE:X64 /DEF:"{ssl_def}" /OUT:extern\\client\\lib\\libssl_x64.lib',
        f'lib.exe /nologo /MACHINE:X64 /DEF:"{crypto_def}" /OUT:extern\\client\\lib\\libcrypto_x64.lib',
    ])
    for exp_file in (ROOT / "extern/client/lib").glob("lib*_x64.exp"):
        exp_file.unlink()


def verify_all() -> int:
    failures = []
    required_libs = [
        "extern/client/lib/cryptlib-x64.lib",
        "extern/client/lib/lzo2_x64.lib",
        "extern/client/lib/libjpeg-x64.lib",
        "extern/client/lib/discord_rpc_x64.lib",
        "extern/client/lib/d3dx9.lib",
        "extern/client/lib/d3dx9d.lib",
        "extern/client/lib/libssl_x64.lib",
        "extern/client/lib/libcrypto_x64.lib",
    ]
    for rel in required_libs:
        p = ROOT / rel
        if not p.exists():
            failures.append(f"MISSING LIB: {rel}")
            continue
        machs = inspect_coff_archive_machines(p)
        if machs != {0x8664}:
            failures.append(f"WRONG ARCH in {rel}: {[hex(m) for m in machs]}")
        else:
            h = hashlib.sha256(p.read_bytes()).hexdigest()[:16]
            print(f"[PASS] {rel} (COFF 0x8664 AMD64, {p.stat().st_size} bytes, sha256={h}...)")

    required_dlls = [
        "extern/client/dll/D3DX9_43.dll",
        "extern/client/dll/libssl-3-x64.dll",
        "extern/client/dll/libcrypto-3-x64.dll",
    ]
    forbidden_mingw_dlls = {"libwinpthread-1.dll", "libstdc++-6.dll", "libgcc_s_seh-1.dll", "libgcc_s_dw2-1.dll"}
    for rel in required_dlls:
        p = ROOT / rel
        if not p.exists():
            failures.append(f"MISSING DLL: {rel}")
            continue
        b = p.read_bytes()
        pe_off = struct.unpack_from("<I", b, 0x3C)[0]
        mach = struct.unpack_from("<H", b, pe_off + 4)[0]
        magic = struct.unpack_from("<H", b, pe_off + 24)[0]
        if mach != 0x8664 or magic != 0x020B:
            failures.append(f"WRONG PE ARCH in {rel}: mach=0x{mach:04X}, magic=0x{magic:04X}")
            continue
        imps = parse_pe_imports(p)
        bad_imps = [imp for imp in imps if imp.lower() in forbidden_mingw_dlls]
        if bad_imps:
            failures.append(f"FORBIDDEN MINGW RUNTIME IMPORT in {rel}: {bad_imps}")
            continue
        h = hashlib.sha256(b).hexdigest()[:16]
        print(f"[PASS] {rel} (PE32+ 0x8664, imports={imps}, sha256={h}...)")

    if failures:
        for f in failures:
            print(f"[FAIL] {f}")
        return 1
    print("[PASS] All client x64 SDK static/import libraries and runtime DLLs verified!")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--target",
        choices=["all", "fast", "cryptopp", "lzo", "libjpeg", "discord_rpc", "openssl"],
        default="fast",
    )
    ap.add_argument("--verify-only", action="store_true")
    args = ap.parse_args()

    if args.verify_only:
        return verify_all()

    if args.target in ("all", "cryptopp"):
        build_cryptopp_x64()
    if args.target in ("all", "fast", "lzo"):
        build_lzo_x64()
    if args.target in ("all", "fast", "libjpeg"):
        build_libjpeg_x64()
    if args.target in ("all", "fast", "discord_rpc"):
        build_discord_rpc_x64()
    if args.target in ("all", "fast", "openssl"):
        build_openssl_x64()

    return verify_all()


if __name__ == "__main__":
    sys.exit(main())

