#!/usr/bin/env python3
"""
MT2009 SDK Inventory, Version Parity, Architecture & Unused-SDK Drift Verifier.
Verifies the repository against tests/sdk_inventory_lock.json.
"""

import hashlib
import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LOCK_PATH = ROOT / "tests" / "sdk_inventory_lock.json"

BINARY_EXTENSIONS = {".lib", ".a", ".dll", ".so", ".obj", ".pyd", ".exp", ".pdb"}


def sha256_file(p: Path) -> str:
    h = hashlib.sha256()
    with p.open("rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def detect_arch(p: Path) -> str:
    data = p.read_bytes()
    if len(data) < 8:
        return "EMPTY"
    if data[:4] == b"\x7fELF":
        ei_class = data[4]
        e_machine = struct.unpack_from("<H", data, 18)[0] if len(data) >= 20 else 0
        return f"ELF{'64' if ei_class == 2 else '32'}_0x{e_machine:04X}"
    if data[:2] == b"MZ" and len(data) >= 0x40:
        pe_off = struct.unpack_from("<I", data, 0x3C)[0]
        if pe_off + 26 <= len(data) and data[pe_off : pe_off + 4] == b"PE\x00\x00":
            mach = struct.unpack_from("<H", data, pe_off + 4)[0]
            magic = struct.unpack_from("<H", data, pe_off + 24)[0]
            pe_kind = (
                "PE32+"
                if magic == 0x20B
                else ("PE32" if magic == 0x10B else f"PE_0x{magic:04X}")
            )
            return f"{pe_kind}_0x{mach:04X}"
    if data[:8] == b"!<arch>\n":
        pos = 8
        machines = set()
        while pos + 60 <= len(data):
            hdr = data[pos : pos + 60]
            try:
                size = int(hdr[48:58].decode("ascii", "ignore").strip() or "0")
            except ValueError:
                break
            body = data[pos + 60 : pos + 60 + size]
            if len(body) >= 20:
                if body[:4] == b"\x7fELF":
                    ei_class = body[4]
                    e_machine = struct.unpack_from("<H", body, 18)[0]
                    machines.add(
                        f"ELF{'64' if ei_class == 2 else '32'}_0x{e_machine:04X}"
                    )
                else:
                    sig1, sig2, ver, mach = struct.unpack_from("<HHHH", body, 0)
                    if sig1 == 0x0000 and sig2 == 0xFFFF:
                        machines.add(f"IMPORT_0x{mach:04X}")
                    elif sig1 in (0x014C, 0x8664):
                        machines.add(f"COFF_0x{sig1:04X}")
            pos += 60 + size + (size % 2)
        return "+".join(sorted(machines)) if machines else "ARCH_UNKNOWN"
    sig = struct.unpack_from("<H", data, 0)[0]
    if sig in (0x014C, 0x8664):
        return f"COFF_0x{sig:04X}"
    return "OTHER"


def main() -> int:
    if not LOCK_PATH.exists():
        print(f"[FAIL] Lockfile missing: {LOCK_PATH}", file=sys.stderr)
        return 1

    lock = json.loads(LOCK_PATH.read_text(encoding="utf-8"))
    passed = 0
    failed = 0

    def check(cond: bool, msg: str):
        nonlocal passed, failed
        if cond:
            passed += 1
        else:
            failed += 1
            print(f"[FAIL] {msg}", file=sys.stderr)

    print("=== MT2009 SDK Inventory & Drift Verification ===")

    # 1. Verify Server/Client Header Parity Pairs
    for pair in lock.get("parity_pairs", []):
        dir_a = ROOT / pair["dir_a"]
        dir_b = ROOT / pair["dir_b"]
        check(dir_a.is_dir(), f"Parity dir_a exists: {pair['dir_a']}")
        check(dir_b.is_dir(), f"Parity dir_b exists: {pair['dir_b']}")
        if dir_a.is_dir() and dir_b.is_dir():
            files_a = {
                f.relative_to(dir_a).as_posix(): sha256_file(f)
                for f in dir_a.rglob("*")
                if f.is_file()
            }
            files_b = {
                f.relative_to(dir_b).as_posix(): sha256_file(f)
                for f in dir_b.rglob("*")
                if f.is_file()
            }
            check(
                len(files_a) == pair["expected_file_count"],
                f"{pair['name']} file count == {pair['expected_file_count']} (got {len(files_a)})",
            )
            check(
                files_a == files_b,
                f"{pair['name']} byte-for-byte parity between {pair['dir_a']} and {pair['dir_b']}",
            )

    # 2. Verify Recorded Dependencies & Binaries
    recorded_binaries = set()
    for dep in lock.get("dependencies", []):
        dep_id = dep["id"]
        check(
            dep.get("decision") in {"VERIFIED", "UPGRADED", "BLOCKED"},
            f"Dependency {dep_id} has valid decision ({dep.get('decision')})",
        )
        for b in dep.get("binaries", []):
            rel = b["path"]
            recorded_binaries.add(rel)
            p = ROOT / rel
            check(p.is_file(), f"[{dep_id}] Binary exists: {rel}")
            if not p.is_file():
                continue
            actual_arch = detect_arch(p)
            actual_sha = sha256_file(p)
            actual_size = p.stat().st_size
            check(
                actual_arch == b["arch"],
                f"[{dep_id}] {rel} arch == {b['arch']} (got {actual_arch})",
            )
            check(
                actual_size == b["size_bytes"],
                f"[{dep_id}] {rel} size == {b['size_bytes']} (got {actual_size})",
            )
            check(
                actual_sha == b["sha256"],
                f"[{dep_id}] {rel} sha256 match",
            )
            if b["role"] == "active_x64":
                check(
                    "0x8664" in actual_arch and "0x014C" not in actual_arch,
                    f"[{dep_id}] active_x64 binary {rel} is pure AMD64 (0x8664), got {actual_arch}",
                )

    # 3. Verify Zero Unrecorded Binaries Under extern/
    extern_root = ROOT / "extern"
    unrecorded = []
    for f in extern_root.rglob("*"):
        if f.is_file() and f.suffix.lower() in BINARY_EXTENSIONS:
            rel = f.relative_to(ROOT).as_posix()
            if rel not in recorded_binaries:
                unrecorded.append(rel)
    check(
        len(unrecorded) == 0,
        f"Zero unrecorded binaries in extern/ (found {len(unrecorded)}: {unrecorded[:5]})",
    )

    # 4. Verify Removed SDKs and Shadowing Files Remain Absent
    for rem in lock.get("removed_sdks", []):
        for rel in rem.get("removed_paths", []):
            p = ROOT / rel
            check(
                not p.exists(),
                f"Removed SDK path remains absent: {rel} ({rem['id']})",
            )

    # Also ensure zero loose .obj files in extern/client/lib
    loose_objs = list((ROOT / "extern/client/lib").glob("*.obj"))
    check(
        len(loose_objs) == 0,
        f"Zero loose .obj files in extern/client/lib (found {len(loose_objs)})",
    )

    # 5. Source Invariant Checks (ThemidaSDK guards + MarkImage Pixel width)
    unguarded_themida = []
    for cpp in (ROOT / "client/src").rglob("*.cpp"):
        text = cpp.read_text(encoding="utf-8", errors="ignore")
        lines = text.splitlines()
        for idx, line in enumerate(lines):
            if re.match(r"^\s*#\s*include\s*[<\"].*ThemidaSDK\.h[>\"]", line):
                window = "\n".join(lines[max(0, idx - 3) : idx])
                if "__THEMIDA__" not in window:
                    unguarded_themida.append(f"{cpp.relative_to(ROOT).as_posix()}:{idx+1}")
    check(
        len(unguarded_themida) == 0,
        f"All #include <ThemidaSDK.h> in client/src are guarded by __THEMIDA__ (unguarded: {unguarded_themida})",
    )

    srv_mark = (ROOT / "server/src/game/src/MarkImage.h").read_text(encoding="utf-8", errors="ignore")
    cli_mark = (ROOT / "client/src/UserInterface/MarkImage.h").read_text(encoding="utf-8", errors="ignore")
    check("typedef uint32_t Pixel;" in srv_mark, "server MarkImage.h uses typedef uint32_t Pixel;")
    check("typedef uint32_t Pixel;" in cli_mark, "client MarkImage.h uses typedef uint32_t Pixel;")

    print(
        f"=== SDK Inventory & Drift Verification: {passed}/{passed + failed} PASS ({failed} failed) ==="
    )
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
