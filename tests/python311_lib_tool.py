#!/usr/bin/env python3
"""client/files/lib inventory, deterministic CPython 3.11 lib builder and verifier.

Commands (run with CPython 3.11.9 x64):
  inventory <dir> [--json out.json]     file kind / magic / PE machine / SHA-256 per file
  build --out <dir> --scripts <dir>...  build py3.11 lib (sourceless legacy-layout .pyc + x64 .pyd) + manifest
  verify <dir>                          every .pyc magic == running interpreter, every PE is AMD64, manifest hashes
Magic numbers are read from the first 4 bytes, never guessed from the extension.
"""
import argparse, ast, hashlib, importlib.util, json, py_compile, shutil, struct, sys
from pathlib import Path

MAGIC_NAMES = {62211: "python2.7", 3413: "python3.8", 3425: "python3.9",
               3439: "python3.10", 3495: "python3.11", 3531: "python3.12"}
STDLIB = Path(sys.base_prefix) / "Lib"
DLLS = Path(sys.base_prefix) / "DLLs"
DENY = {"_hashlib", "_ssl", "_tkinter", "tkinter", "_sqlite3", "sqlite3", "_ctypes", "ctypes", "_testcapi",
        "_testbuffer", "_testinternalcapi", "_testmultiphase", "_testimportmultiple", "_testconsole",
        "_ctypes_test", "_msi", "_multiprocessing", "idlelib", "test", "turtledemo", "turtle", "ensurepip",
        "lib2to3", "distutils", "pydoc_data", "unittest", "doctest", "pdb", "asyncio", "_asyncio", "_overlapped",
        "multiprocessing", "concurrent", "email", "http", "xmlrpc", "ftplib", "smtplib", "imaplib",
        "poplib", "nntplib", "telnetlib", "socketserver", "wsgiref", "venv", "zipapp", "curses"}
ALWAYS = ["encodings", "codecs", "io", "abc", "os", "stat", "ntpath", "genericpath", "_collections_abc",
          "types", "warnings", "functools", "collections", "keyword", "operator", "reprlib", "heapq", "re",
          "enum", "sre_compile", "sre_parse", "sre_constants", "copyreg", "linecache", "tokenize", "token",
          "traceback", "weakref", "_weakrefset", "locale", "string", "struct", "threading", "random", "bisect",
          "hashlib", "base64", "binascii", "copy", "decimal", "_pydecimal", "numbers", "difflib", "fnmatch",
          "shutil", "shlex", "subprocess", "sysconfig", "uuid", "json", "importlib", "importlib.util",
          "importlib.machinery", "importlib.abc", "datetime", "time", "math", "errno", "select", "selectors",
          "socket", "unicodedata", "xml.dom.minidom", "xml.parsers.expat", "__future__", "textwrap", "pprint", "webbrowser", "site", "collections.abc", "json.decoder", "json.encoder",
          "json.scanner", "encodings.aliases"]

def sha256(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def pe_machine(p):
    d = Path(p).read_bytes()
    if d[:2] != b"MZ" or len(d) < 0x40:
        return "not_pe"
    off = struct.unpack_from("<I", d, 0x3C)[0]
    if d[off:off + 4] != b"PE\0\0":
        return "bad_pe"
    return {0x8664: "AMD64", 0x14C: "I386", 0xAA64: "ARM64"}.get(struct.unpack_from("<H", d, off + 4)[0], "other")


def classify(p):
    p = Path(p)
    ext = p.suffix.lower()
    rec = {"size": p.stat().st_size, "sha256": sha256(p)}
    if ext in (".pyc", ".pyo"):
        d = p.read_bytes()
        if len(d) < 16:
            rec.update(kind="pyc", verdict="CORRUPT_TOO_SHORT")
            return rec
        magic = struct.unpack_from("<H", d, 0)[0]
        rec.update(kind="pyc", magic=magic, crlf=d[2:4] == b"\r\n", version=MAGIC_NAMES.get(magic, "unknown"))
        rec["verdict"] = ("OK_CURRENT" if d[:4] == importlib.util.MAGIC_NUMBER else
                          "PY2_BYTECODE" if magic == 62211 else "OTHER_VERSION" if d[2:4] == b"\r\n" else "CORRUPT_MAGIC")
    elif ext in (".pyd", ".dll"):
        m = pe_machine(p)
        rec.update(kind="pe", machine=m, verdict="OK_AMD64" if m == "AMD64" else "NOT_AMD64")
    elif ext == ".py":
        rec.update(kind="py", verdict="SOURCE")
    else:
        rec.update(kind="other", verdict="OTHER")
    return rec


def walk(root):
    root = Path(root)
    return {str(p.relative_to(root)).replace("\\", "/"): classify(p) for p in sorted(root.rglob("*")) if p.is_file()}


def cmd_inventory(a):
    inv = walk(a.dir)
    counts = {}
    for r in inv.values():
        key = f'{r["kind"]}:{r["verdict"]}'
        counts[key] = counts.get(key, 0) + 1
    print("INVENTORY files=%d" % len(inv), json.dumps(counts, sort_keys=True))
    if a.json:
        Path(a.json).write_text(json.dumps({"files": inv, "counts": counts}, indent=1, sort_keys=True), encoding="utf-8")
    return 0


def find_module(name):
    """Return (kind, path) for a stdlib module/package, kind in src/pkg/pyd/None."""
    parts = name.split(".")
    base = STDLIB.joinpath(*parts)
    if base.is_dir() and (base / "__init__.py").exists():
        return "pkg", base / "__init__.py"
    if base.with_suffix(".py").exists():
        return "src", base.with_suffix(".py")
    if len(parts) == 1 and (DLLS / (name + ".pyd")).exists():
        return "pyd", DLLS / (name + ".pyd")
    return None, None


def imports_of(path, modname, ispkg):
    out = set()
    try:
        tree = ast.parse(Path(path).read_text(encoding="utf-8"))
    except (SyntaxError, UnicodeDecodeError):
        return out
    pkg = modname if ispkg else modname.rpartition(".")[0]
    for n in ast.walk(tree):
        if isinstance(n, ast.Import):
            out.update(a.name for a in n.names)
        elif isinstance(n, ast.ImportFrom):
            if n.level:
                parts = pkg.split(".") if pkg else []
                parts = parts[:len(parts) - (n.level - 1)] if n.level > 1 else parts
                base = ".".join(parts + ([n.module] if n.module else []))
            else:
                base = n.module or ""
            if base:
                out.add(base)
                out.update(base + "." + a.name for a in n.names if a.name != "*")
    return out


def closure(seeds):
    todo, seen, plan, unresolved = list(seeds), set(), {}, set()
    while todo:
        name = todo.pop()
        if name in seen:
            continue
        seen.add(name)
        parts = name.split(".")
        if parts[0] in DENY or name in DENY:
            continue
        for i in range(1, len(parts)):
            todo.append(".".join(parts[:i]))
        if parts[0] in sys.builtin_module_names or parts[0] in ("sys", "builtins", "marshal", "winreg", "_weakref"):
            continue
        kind, path = find_module(name)
        if kind is None:
            if "." not in name or find_module(name.rpartition(".")[0])[0] == "pkg":
                unresolved.add(name)
            continue
        plan[name] = (kind, path)
        if kind != "pyd":
            todo.extend(imports_of(path, name, kind == "pkg"))
    return plan, unresolved


def cmd_build(a):
    out = Path(a.out)
    if out.exists():
        print("refusing to overwrite existing", out)
        return 3
    seeds = set(ALWAYS)
    for sd in a.scripts:
        for p in Path(sd).rglob("*.py"):
            seeds.update(imports_of(p, "", False))
    local = {p.stem for sd in a.scripts for p in Path(sd).rglob("*.py")}
    seeds = {s for s in seeds if s.split(".")[0] not in local}
    # every codec module: Turkish/Polish/Korean/CJK code pages must be loadable
    seeds.update("encodings." + p.stem for p in (STDLIB / "encodings").glob("*.py") if p.stem != "__init__")
    plan, unresolved = closure(seeds)
    manifest = {"python": sys.version, "magic": importlib.util.MAGIC_NUMBER.hex(), "modules": {}, "denied_unresolved": sorted(unresolved)}
    for name, (kind, path) in sorted(plan.items()):
        rel = Path(*name.split("."))
        if kind == "pyd":
            dst = out / (name + ".pyd")
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, dst)
        else:
            dst = out / (rel / "__init__.pyc" if kind == "pkg" else rel.with_suffix(".pyc"))
            dst.parent.mkdir(parents=True, exist_ok=True)
            py_compile.compile(str(path), cfile=str(dst), dfile="lib/" + str(dst.relative_to(out)).replace("\\", "/")[:-1],
                               doraise=True, invalidation_mode=py_compile.PycInvalidationMode.CHECKED_HASH)
    # simplejson: project-vendored source (round-tripped through the same sourceless pyc path)
    if a.simplejson_src:
        for p in sorted(Path(a.simplejson_src).glob("*.py")):
            if p.name == "tool.py":
                continue
            dst = out / "simplejson" / (p.stem + ".pyc")
            dst.parent.mkdir(parents=True, exist_ok=True)
            py_compile.compile(str(p), cfile=str(dst), dfile="lib/simplejson/" + p.name, doraise=True,
                               invalidation_mode=py_compile.PycInvalidationMode.CHECKED_HASH)
    files = walk(out)
    manifest["files"] = {k: {"sha256": v["sha256"], "size": v["size"], "verdict": v["verdict"]} for k, v in files.items()}
    (out / "MANIFEST.json").write_text(json.dumps(manifest, indent=1, sort_keys=True), encoding="utf-8")
    print("BUILD files=%d modules=%d unresolved_or_denied=%d out=%s" % (len(files), len(plan), len(unresolved), out))
    return 0


def cmd_verify(a):
    root = Path(a.dir)
    inv = walk(root)
    mf = json.loads((root / "MANIFEST.json").read_text(encoding="utf-8")) if (root / "MANIFEST.json").exists() else None
    bad = [(k, v["verdict"]) for k, v in inv.items() if v["verdict"] in ("PY2_BYTECODE", "OTHER_VERSION", "CORRUPT_MAGIC",
           "CORRUPT_TOO_SHORT", "NOT_AMD64")]
    npyc = sum(1 for v in inv.values() if v["kind"] == "pyc")
    npe = sum(1 for v in inv.values() if v["kind"] == "pe")
    mism = []
    if mf:
        for k, v in mf["files"].items():
            if k not in inv or inv[k]["sha256"] != v["sha256"]:
                mism.append(k)
    print("VERIFY pyc=%d pe=%d bad=%d manifest_mismatch=%d manifest=%s" % (npyc, npe, len(bad), len(mism), bool(mf)))
    for b in bad[:20]:
        print("  BAD", b)
    return 0 if not bad and not mism and mf else 1


def main():
    ap = argparse.ArgumentParser()
    sp = ap.add_subparsers(dest="cmd", required=True)
    s = sp.add_parser("inventory"); s.add_argument("dir"); s.add_argument("--json")
    s = sp.add_parser("build"); s.add_argument("--out", required=True); s.add_argument("--scripts", nargs="+", required=True)
    s.add_argument("--simplejson-src")
    s = sp.add_parser("verify"); s.add_argument("dir")
    a = ap.parse_args()
    if sys.version_info[:3] != (3, 11, 9) or struct.calcsize("P") != 8:
        print("requires CPython 3.11.9 x64, got", sys.version)
        return 2
    return {"inventory": cmd_inventory, "build": cmd_build, "verify": cmd_verify}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
