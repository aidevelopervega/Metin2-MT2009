#!/usr/bin/env python3
"""MT2009 - Python 3.11.9 gecis araci: envanter / donusum / dogrulama.

Orijinal arsivler ASLA degistirilmez. Calisma alani: build/python311_migration
  orig_pack/    orijinal root/locale .data/.index KOPYALARI
  extracted/    motorun kendi CEterPack kodu ile cikarilan (gercek yukleyici kaynagi) dosyalar
  converted/    donusturulmus dosyalar
  manifest/     JSON kayitlari (hash, kategori, encoding, derleme sonucu, degisiklik kaydi)

Komutlar:  inventory | convert | verify-compile
Gereken: CPython 3.11.x (lib2to3 3.11'de mevcuttur; 3.13'te kaldirilmistir).
"""
import argparse
import ast
import collections
import hashlib
import importlib.util
import json
import os
import pathlib
import platform
import re
import struct
import subprocess
import sys
import time
import warnings

warnings.simplefilter("ignore")

ROOT = pathlib.Path(__file__).resolve().parent.parent
WORK = ROOT / "build" / "python311_migration"
CLIENT_FILES = ROOT / "client" / "files"
AREAS = ("root", "locale")

REQUIRED_PYTHON_VERSION = (3, 11, 9)
REQUIRED_PYTHON_IMPL = "CPython"
REQUIRED_PTR_SIZE = 8
REQUIRED_MACHINES = ("AMD64", "X86_64")
REQUIRED_MAGIC_HEX = "a70d0d0a"  # CPython 3.11 magic 3495

PACK_ARCHIVE_NAMES = ("root.index", "root.data", "locale.index", "locale.data")

EXPECTED_ORIG_PACK_SHA256 = {
    "root.index": "942c1852aad09f096a5a9cd3fcfa476dca28f85f893058057c5a2791c6d01d4f",
    "root.data": "369dc30bcf91052ba092b41f489f3fd174ca7dbf3bc6dd1df5b7ffd6ef81eef5",
    "locale.index": "ac6876fa99b707efde87fdc4ef135dd661594f2c0322836e92657b46c9db9022",
    "locale.data": "6e44d594ff9e076bc1e8b0de98c892a2e024a20bcfad237f4e1f724a6453ec43",
}

EXPECTED_PY311_OUT2_SHA256 = {
    "root.index": "49b5d82b2f61676c75f65d425e357de51fa6fd1bdd93e1815b77ff67391d511b",
    "root.data": "002d65cc6a4fb089efb3d9e9ccf23ab19079c504677b2beea58940aaa0474355",
    "locale.index": "ae9fb3708915b5ad2f1786c6280cdf5f13ca9cd463e2088a14374a0d3d440263",
    "locale.data": "1ad610838c001982b07535249827fbd716123f6f60e4a0fd6b5706ad55a0a9d0",
}

COMPATIBILITY_CONTRACT = {
    "py311_x64_client_runtime": {
        "embedded_python": "CPython 3.11.9 x64 (PE32+ AMD64, MSC v.1938 64 bit)",
        "required_client_files_lib": "270 sourceless PEP-552 CHECKED_HASH .pyc (magic 0xa70d0d0a / 3495) + 8 PE32+ AMD64 .pyd (total 278 modules)",
        "required_client_files_pack": "out2/{root,locale}.{index,data} (Python 3.11 syntax, division & behavioral equivalence patches)",
        "compatible_with_orig_pack_py27": False,
        "compatible_with_lib_py27_backup": False,
    },
    "py27_x86_legacy_baseline": {
        "embedded_python": "CPython 2.7.6 x86 (PE32 I386)",
        "required_client_files_lib": "lib_py27_backup (70 py2.7 .pyc magic 62211 + 2 corrupt .pyc + 2 py3 .pyc + 2 I386 pyexpat.pyd + 9 simplejson .py = 85 files)",
        "required_client_files_pack": "orig_pack/{root,locale}.{index,data} (Python 2.7 syntax)",
        "compatible_with_py311_lib": False,
        "compatible_with_py311_out2_pack": False,
    },
    "rollback_packs_scope": {
        "restores": list(PACK_ARCHIVE_NAMES),
        "modifies_client_files_lib": False,
        "restored_pack_format": "python27_orig_pack",
        "is_working_py311_client_rollback": False,
        "note": (
            "rollback-packs yalnizca 4 EterPack arsivini (root.index, root.data, locale.index, locale.data) "
            "dagitim oncesi orijinal Python 2.7 (orig_pack) durumuna geri yukler; client/files/lib dizinini degistirmez. "
            "Python 2.7 orig_pack arsivlerine geri donus, gomulu CPython 3.11.9 istemcisi icin calisan bir "
            "calisma zamani rollback'i DEGILDIR (CPython 3.11.9 istemcisi hem Python 3.11 lib hem Python 3.11 out2 paketlerini zorunlu kilar)."
        ),
    },
}


class VersionLockError(RuntimeError):
    """Yanlis Python surumu, yorumlayici uygulamasi veya mimari tespit edildiginde firlatilir."""


class DeploymentSafetyError(RuntimeError):
    """SHA-256, yedek butunlugu, islem gunlugu veya kopyalama hatasi tespit edildiginde firlatilir."""


def check_python_environment(version_info=None, implementation=None, ptr_size=None, machine=None, magic_hex=None):
    sim = os.environ.get("MT2_SIMULATE_PYTHON_ENV", "").strip()
    if sim and version_info is None:
        # Format: "3.11.8,CPython,8,AMD64,a70d0d0a"
        parts = [p.strip() for p in sim.split(",")]
        ver_parts = [int(x) for x in parts[0].split(".")]
        version_info = tuple(ver_parts)
        implementation = parts[1] if len(parts) > 1 else REQUIRED_PYTHON_IMPL
        ptr_size = int(parts[2]) if len(parts) > 2 else REQUIRED_PTR_SIZE
        machine = parts[3] if len(parts) > 3 else "AMD64"
        magic_hex = parts[4] if len(parts) > 4 else REQUIRED_MAGIC_HEX
    else:
        if version_info is None:
            version_info = tuple(sys.version_info[:3])
        if implementation is None:
            implementation = platform.python_implementation()
        if ptr_size is None:
            ptr_size = struct.calcsize("P")
        if machine is None:
            machine = platform.machine()
        if magic_hex is None:
            magic_hex = importlib.util.MAGIC_NUMBER.hex()

    if tuple(version_info[:3]) != REQUIRED_PYTHON_VERSION:
        raise VersionLockError(
            f"Python surum kilidi hatasi: yalnizca CPython 3.11.9 zorunludur, bulunan surum={version_info[:3]}"
        )
    if implementation != REQUIRED_PYTHON_IMPL:
        raise VersionLockError(
            f"Python yorumlayici kilidi hatasi: yalnizca CPython zorunludur, bulunan={implementation!r}"
        )
    if ptr_size != REQUIRED_PTR_SIZE or str(machine).upper() not in REQUIRED_MACHINES:
        raise VersionLockError(
            f"Mimari kilidi hatasi: yalnizca 64-bit AMD64 (sizeof(void*)==8) zorunludur, bulunan ptr_size={ptr_size} machine={machine!r}"
        )
    if str(magic_hex).lower() != REQUIRED_MAGIC_HEX:
        raise VersionLockError(
            f"Bytecode magic kilidi hatasi: beklenen 0x{REQUIRED_MAGIC_HEX} (3495), bulunan=0x{magic_hex}"
        )
    return {
        "version": list(version_info[:3]),
        "implementation": implementation,
        "ptr_size": ptr_size,
        "machine": machine,
        "magic_hex": magic_hex,
    }


def enforce_cpython_3119_x64():
    try:
        return check_python_environment()
    except VersionLockError as exc:
        sys.stderr.write(f"FATAL: {exc}\n")
        sys.exit(2)

# lib2to3 donusturuculerinden KASITLI olarak cikarilanlar:
#  import   -> "import ui" satirlarini goreli ice aktarmaya cevirir; Metin2 ozel yukleyicisini bozar.
#  execfile -> system.py'nin ozel (EterPack-bilincli) execfile() cagrisini open()+compile()'a cevirip atlatir.
EXCLUDED_FIXERS = {"import", "execfile"}

CATEGORY_RULES = [  # ilk eslesen kazanir
    ("Locale", r"^(locale/|localeinfo|uiscriptlocale)"),
    ("UI definitions", r"^uiscript/"),
    ("Login/Server/CharSelect", r"^(intro|networkmodule|serverinfo|servercommandparser)"),
    ("Game startup", r"^(system|prototype|playersettingmodule|debuginfo|rootlibcythonizer|configmain|uiphasecurtain|exception)\.py$"),
    ("Quest", r"^uiquest"),
    ("Trade/shop", r"^(uishop|uiexchange|uiprivateshop|offlineshop|uiikashop|uiitemshop|itemshop_|uispecialshop|uiwonexchange|uicraft|uicube|uiitemexchange)"),
    ("Guild/party", r"^(uiguild|uiparty|uimessenger|uiuploadmark)"),
    ("Chat", r"^(uichat|uiwhisper|uireport)"),
    ("Settings", r"^(uigameoption|uioption|uisystem|gamesettings|uisystemoption)"),
    ("Minimap", r"^uiminimap"),
    ("Inventory/equipment", r"^(uiinventory|uihorseinventory|uiequipmentdialog|uiattachmetin|uiattributelist|uiacce|uirefine|uipickmoney|uipicketc|uiselectitem|uisafebox)"),
    ("Skills/combat", r"^(uicharacter|uipointreset|uipotionrecharge|uidragonsoul|dragon_soul|uiplayerstat|uiaffectbar|special_flags|uitarget|uiplayergauge|uifishing)"),
    ("World/gameplay", r"^(game|interfacemodule|uifloatingtext|uimapnameshower|uigamebutton|consolemodule|uigamemaster|cinemachine|flamewindpath|eventmanager|emotion|updateable|uirestart|uimaintenance|uicaptcha|uiautoban|uibusyaction|uiglobalrankingsmanager|uiworldboss|uireputation|uiweb|uihelp|uiselectmusic|uicandidate|test_affect|musicinfo|uimovechannel)"),
    ("Shared libraries", r"^(ui|ui_event|utils|constinfo|colorinfo|mathutils|tween|tweencamera|file_utils|hash_utils|mousemodule|gamefont|uicommon|uiex|uitip|uitooltip|gamerules)\.py$"),
]


def sha256(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


def classify(area: str, rel: str) -> str:
    key = f"locale/{rel}" if area == "locale" and not rel.startswith("locale/") else rel
    if area == "locale":
        return "Locale" if not rel.startswith("locale/common/ui") and not rel.startswith("locale/pl/ui") else "UI definitions"
    for cat, pat in CATEGORY_RULES:
        if re.search(pat, key):
            return cat
    return "Other"


def exec_kind(area: str, rel: str) -> str:
    """UI sozluk/script dosyalari import edilmez; ui.PythonScriptLoader/execfile ile calisir."""
    if area == "locale" or rel.startswith("uiscript/"):
        return "ui_definition_execfile"
    return "module_import"


def encoding_class(raw: bytes) -> str:
    if all(c < 0x80 for c in raw):
        return "ascii"
    try:
        raw.decode("utf-8")
        return "utf8"
    except UnicodeDecodeError:
        pass
    try:
        raw.decode("cp949")
        return "cp949"
    except UnicodeDecodeError:
        return "unknown"


def decode_source(raw: bytes):
    enc = encoding_class(raw)
    if enc in ("ascii", "utf8"):
        return raw.decode("utf-8"), enc
    if enc == "cp949":
        return raw.decode("cp949"), enc
    raise ValueError("bilinmeyen kodlama")


def try_compile(src, name: str):
    try:
        compile(src, name, "exec")
        return None
    except Exception as e:  # noqa: BLE001
        return f"{type(e).__name__}: {e}"


def iter_py(area: str, base: pathlib.Path):
    for p in sorted((base / area).rglob("*.py")):
        yield p.relative_to(base / area).as_posix(), p


def cmd_inventory(a):
    ext = WORK / "extracted"
    unp = ROOT / "client" / "files" / "pack"
    recs = []
    cmp_stats = collections.Counter()
    for area in AREAS:
        files = {p.relative_to(ext / area).as_posix(): p for p in sorted((ext / area).rglob("*")) if p.is_file()}
        unpacked = {}
        ud = unp / area
        if ud.is_dir():
            unpacked = {p.relative_to(ud).as_posix(): p for p in ud.rglob("*") if p.is_file()}
        for rel, p in files.items():
            raw = p.read_bytes()
            r = {"area": area, "path": rel, "size": len(raw), "sha256": sha256(raw)}
            if rel in unpacked:
                same = sha256(unpacked[rel].read_bytes()) == r["sha256"]
                r["unpacked_dir"] = "identical" if same else "DIFFERENT"
            else:
                r["unpacked_dir"] = "absent"
            cmp_stats[(area, r["unpacked_dir"])] += 1
            if rel.endswith(".py"):
                enc = encoding_class(raw)
                r.update(kind=exec_kind(area, rel), category=classify(area, rel), encoding=enc)
                try:
                    text, _ = decode_source(raw)
                    r["py311_compile_original"] = try_compile(text.replace("\r\n", "\n"), rel) or "OK"
                except ValueError:
                    r["py311_compile_original"] = "UNDECODABLE"
            recs.append(r)
        only_unpacked = [k for k in unpacked if k not in files and k.endswith(".py")]
        cmp_stats[(area, "py_only_in_unpacked_dir")] = len(only_unpacked)
    (WORK / "manifest").mkdir(parents=True, exist_ok=True)
    (WORK / "manifest" / "inventory.json").write_text(json.dumps(recs, indent=1, ensure_ascii=False), encoding="utf-8")
    py = [r for r in recs if r["path"].endswith(".py")]
    cats = collections.defaultdict(lambda: collections.Counter())
    for r in py:
        c = cats[r["category"]]
        c["total"] += 1
        c["py311_compiles_as_is"] += r["py311_compile_original"] == "OK"
        c["non_utf8_cp949"] += r["encoding"] == "cp949"
    print("INVENTORY files=%d py=%d" % (len(recs), len(py)))
    for k, v in sorted(cmp_stats.items()):
        print("  unpacked-dir-vs-archive", k, v)
    for cat, c in sorted(cats.items()):
        print("  %-26s total=%3d compiles_as_is=%3d cp949=%2d" % (cat, c["total"], c["py311_compiles_as_is"], c["non_utf8_cp949"]))
    return 0


def make_tool():
    enforce_cpython_3119_x64()
    from lib2to3 import refactor
    names = [n for n in refactor.get_fixers_from_package("lib2to3.fixes")]
    explicit = {"lib2to3.fixes.fix_idioms", "lib2to3.fixes.fix_set_literal", "lib2to3.fixes.fix_ws_comma", "lib2to3.fixes.fix_buffer"}
    chosen = [n for n in names if n not in explicit and n.rsplit(".fix_", 1)[1] not in EXCLUDED_FIXERS]
    return refactor.RefactoringTool(chosen)


# lib2to3'un cozemedigi / cozdugunde oyun davranisini degistirecegi noktalar icin TAM ESLESMELI yamalar.
# (dosya, eski, yeni, beklenen_adet) -- adet tutmazsa arac hata verir; sessiz uygulanmaz.
_SYS_OPEN_OLD = ("def open(filename, mode = 'rb'):\n\tif pack.Exist(filename) and mode in ('r', 'rb'):\n"
                 "\t\treturn pack_file(filename, mode)\n\telse:\n\t\treturn old_open(filename, mode)\n")
# mode=None: paketten okuma Py2'deki gibi 'rb' kalir; diskten okumada Py3 yerlesik open() varsayilani ('r', metin) kullanilir
# (Py2'de 'rb' str dondurur; Py3'te ayni kod 'rb' ile bytes alir -> sessiz davranis farki).
_SYS_OPEN_NEW = ("def open(filename, mode = None, *args, **kwargs):\n\tif isinstance(filename, str) and pack.Exist(filename) and mode in (None, 'r', 'rb'):\n"
                 "\t\treturn pack_file(filename, mode or 'rb')\n\telse:\n\t\treturn old_open(filename, 'r' if mode is None else mode, *args, **kwargs)\n")
_SYS_LOADC_OLD = ("def __LoadCompiledFile__(sFileName):\n\tkFile=open(sFileName)\n\tif kFile.read(4)!=imp.get_magic():\n\t\traise\n\n"
                  "\tkFile.read(4)\n\n\tkData=kFile.read()\n\treturn marshal.loads(kData)\n")
_SYS_LOADC_NEW = ("def __LoadCompiledFile__(sFileName):\n\tkData=open(sFileName, 'rb').read()\n\tif isinstance(kData, str):\n"
                  "\t\tkData=kData.encode('utf-8', 'surrogateescape') # pack.Get() ham baytlari surrogateescape ile str'ye cevirir\n"
                  "\tif kData[:4]!=importlib.util.MAGIC_NUMBER:\n\t\traise ImportError('bad magic number in %s' % sFileName)\n\n"
                  "\t# PEP 552: 4 bayt magic + 4 bayt flags + 8 bayt (mtime,size | kaynak hash'i)\n\treturn marshal.loads(kData[16:])\n")
PATCHES = {
    "system.py": [
        ("import builtins\n\n\nimport imp\nimport marshal\n", "import builtins\n\n\nimport types\nimport marshal\nimport importlib.util\n", 1),
        ("import marshal\nimport imp\nimport pack\n", "import marshal\nimport pack\n", 1),
        ("class TraceFile:\n\tdef write(self, msg):\n\t\tdbg.Trace(msg)\n",
         "class TraceFile:\n\tdef write(self, msg):\n\t\tdbg.Trace(msg)\n\n\tdef flush(self):\n\t\tpass\n", 1),
        ("class TraceErrorFile:\n\tdef write(self, msg):\n\t\tdbg.TraceError(msg)\n\t\tdbg.RegisterExceptionString(msg)\n",
         "class TraceErrorFile:\n\tdef write(self, msg):\n\t\tdbg.TraceError(msg)\n\t\tdbg.RegisterExceptionString(msg)\n\n\tdef flush(self):\n\t\tpass\n", 1),
        ("__builtins__", "builtins", 6),
        (_SYS_OPEN_OLD, _SYS_OPEN_NEW, 1),
        ("module = imp.new_module(fqname)", "module = types.ModuleType(fqname)", 1),
        ("def __hybrid_import(name,globals=None,locals=None,fromlist=None, level=-1):",
         "def __hybrid_import(name,globals=None,locals=None,fromlist=None, level=0):", 1),
        ("\t\t\tamp = imp.find_module(name)[2][0]\n\t\t\tif amp == \"\":\n\t\t\t\treturn old_import(name, globals, locals, fromlist)\n",
         "\t\t\treturn old_import(name, globals, locals, fromlist, level)\n", 1),
        ("return old_import(name,globals,locals,fromlist)#, level)", "return old_import(name,globals,locals,fromlist,level)", 1),
        (_SYS_LOADC_OLD, _SYS_LOADC_NEW, 1),
        ("builtins.old_execfile = builtins.execfile\n", "", 1),
    ],
    "utils.py": [
        ("WHITE_LIST = ['__builtin__', 'types', __name__, '__main__', 'sys']", "WHITE_LIST = ['builtins', 'types', __name__, '__main__', 'sys']", 1),
        ("apply(*self.clickEvent)", "self.clickEvent[0](*self.clickEvent[1])", 3),
        ("\tfor n in value:\n\t\tif n not in nvalue:\n\t\t\tdel value[n]\n",
         "\tfor n in list(value):\n\t\tif n not in nvalue:\n\t\t\tdel value[n]\n", 1),
    ],
    "uiquest.py": [("list(map(apply,z))", "[fn() for fn in z]", 1)],
    "ui.py": [
        ("\t\timport exceptions\n", "", 1),
        ("int(round(y))", "(int(y + 0.5) if y >= 0 else int(y - 0.5))", 1),  # Python 2 round(): .5 sifirdan uzaga
    ],
    "uitooltip.py": [
        ("def chop(n):\n\treturn round(n - 0.5, 1)\n",
         "def chop(n):\n\t_v = (n - 0.5) * 10.0\n\treturn (int(_v + 0.5) if _v >= 0 else int(_v - 0.5)) / 10.0\n", 1),
        ("valueCalc\t= round(value * abs) / 100",
         "valueCalc\t= float(int(value * abs + 0.5) if (value * abs) >= 0 else int(value * abs - 0.5)) / 100", 1),
    ],
}


def apply_patches(norm: str, rel: str, applied: list) -> str:
    for old, new, count in PATCHES.get(rel, []):
        found = norm.count(old)
        if found != count:
            raise ValueError(f"yama eslesmedi {rel}: beklenen {count}, bulunan {found}: {old[:60]!r}")
        norm = norm.replace(old, new)
        applied.append({"file": rel, "old": old[:80], "count": count})
    return norm


def convert_text(tool, text: str, name: str):
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
    from python311_divpass import rewrite_division
    nl = "\r\n" if text.count("\r\n") > text.count("\n") / 2 else "\n"
    norm = text.replace("\r\n", "\n")
    if not norm.endswith("\n"):
        norm += "\n"
    out = str(tool.refactor_string(norm, name))
    applied = []
    out = apply_patches(out, name, applied)
    out, dstats = rewrite_division(out, name)
    return out.replace("\n", nl), nl, applied, dstats


def cmd_convert(a):
    tool = make_tool()
    inv = {(r["area"], r["path"]): r for r in json.loads((WORK / "manifest" / "inventory.json").read_text(encoding="utf-8"))}
    conv = WORK / "converted"
    changelog = []
    fails = []
    for area in AREAS:
        for rel, p in iter_py(area, WORK / "extracted"):
            raw = p.read_bytes()
            text, enc = decode_source(raw)
            try:
                out, nl, applied, dstats = convert_text(tool, text, rel)
            except Exception as e:  # noqa: BLE001
                fails.append((area, rel, f"{type(e).__name__}: {e}"))
                continue
            dst = conv / area / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            dst.write_bytes(out.encode("utf-8"))
            changed = out != text
            changelog.append({"area": area, "path": rel, "src_encoding": enc, "newline": "CRLF" if nl == "\r\n" else "LF",
                              "src_sha256": sha256(raw), "out_sha256": sha256(out.encode("utf-8")),
                              "modified": changed or enc == "cp949", "reencoded_cp949_to_utf8": enc == "cp949",
                              "manual_patches": applied,
                              "division": {"int_floor": dstats["floor"], "float_true": dstats["true"],
                                           "runtime_helper": dstats["helper"], "helper_sites": dstats["helper_sites"],
                                           "manual_review": dstats["manual"]}})
    (WORK / "manifest" / "convert_changelog.json").write_text(json.dumps(changelog, indent=1, ensure_ascii=False), encoding="utf-8")
    dv = [c["division"] for c in changelog]
    print("DIVISION int_floor=%d float_true=%d runtime_helper=%d manual_review=%d patched_files=%d" % (
        sum(d["int_floor"] for d in dv), sum(d["float_true"] for d in dv), sum(d["runtime_helper"] for d in dv),
        sum(len(d["manual_review"]) for d in dv), sum(1 for c in changelog if c["manual_patches"])))
    print("CONVERT files=%d modified=%d reencoded=%d failures=%d" % (
        len(changelog), sum(c["modified"] for c in changelog), sum(c["reencoded_cp949_to_utf8"] for c in changelog), len(fails)))
    for f in fails:
        print("  FAIL", f)
    return 1 if fails else 0


def cmd_compile(a):
    conv = WORK / "converted"
    bad = 0
    n = 0
    for area in AREAS:
        for rel, p in iter_py(area, conv):
            n += 1
            err = try_compile(p.read_bytes().decode("utf-8").replace("\r\n", "\n"), rel)
            if err:
                bad += 1
                print("  COMPILE_FAIL", area, rel, err)
    print("COMPILE files=%d failures=%d" % (n, bad))
    return 1 if bad else 0


def _ensure_hashseed_zero():
    if os.environ.get("PYTHONHASHSEED") != "0":
        env = dict(os.environ, PYTHONHASHSEED="0")
        rc = subprocess.call([sys.executable, str(pathlib.Path(__file__).resolve())] + sys.argv[1:], env=env)
        sys.exit(rc)


def build_lib_into(dst_lib: pathlib.Path, template_manifest_path: pathlib.Path) -> dict:
    import importlib._bootstrap_external as ibe
    import importlib.util
    import shutil

    py311_root = pathlib.Path(sys.base_prefix)
    py311_lib = py311_root / "Lib"
    py311_dlls = py311_root / "DLLs"
    sjson_bak = WORK / "lib_py27_backup" / "simplejson"
    tpl = json.loads(template_manifest_path.read_text(encoding="utf-8"))
    if dst_lib.exists():
        shutil.rmtree(dst_lib)
    dst_lib.mkdir(parents=True, exist_ok=True)
    files_out = {}
    pyc_count = 0
    pyd_count = 0
    for rel in sorted(tpl["files"]):
        out_path = dst_lib / rel
        out_path.parent.mkdir(parents=True, exist_ok=True)
        if rel.endswith(".pyd"):
            raw = (py311_dlls / rel).read_bytes()
            out_path.write_bytes(raw)
            pyd_count += 1
        elif rel.endswith(".pyc"):
            py_rel = rel[:-1]
            if rel.startswith("simplejson/"):
                src_path = sjson_bak / py_rel[len("simplejson/"):]
            else:
                src_path = py311_lib / py_rel
            raw_src = src_path.read_bytes()
            co = compile(raw_src, "lib/" + py_rel, "exec", dont_inherit=True)
            sh = importlib.util.source_hash(raw_src)
            raw = ibe._code_to_hash_pyc(co, sh, checked=True)
            out_path.write_bytes(raw)
            pyc_count += 1
        else:
            continue
        files_out[rel] = {
            "sha256": sha256(raw),
            "size": len(raw),
            "verdict": tpl["files"][rel].get("verdict", "OK_CURRENT"),
        }
    manifest_out = {
        "denied_unresolved": tpl.get("denied_unresolved", []),
        "files": files_out,
        "magic": importlib.util.MAGIC_NUMBER.hex(),
        "modules": tpl.get("modules", {}),
        "python": sys.version,
    }
    (dst_lib / "MANIFEST.json").write_text(json.dumps(manifest_out, indent=1, sort_keys=True), encoding="utf-8")
    return {"pyc": pyc_count, "pyd": pyd_count, "total": pyc_count + pyd_count, "manifest": manifest_out}


def stage_and_repack_into(extracted_dir: pathlib.Path, converted_dir: pathlib.Path, staging_dir: pathlib.Path, out_pack_dir: pathlib.Path) -> dict:
    import shutil
    tool_exe = WORK / "bin" / "eterpack_py_tool.exe"
    if not tool_exe.is_file():
        raise FileNotFoundError(f"Eksik EterPack araci: {tool_exe}")
    if staging_dir.exists():
        shutil.rmtree(staging_dir)
    if out_pack_dir.exists():
        shutil.rmtree(out_pack_dir)
    staging_dir.mkdir(parents=True, exist_ok=True)
    out_pack_dir.mkdir(parents=True, exist_ok=True)
    counts = {}
    for pack_name in ("root", "locale"):
        src_ext = extracted_dir / pack_name
        dst_stg = staging_dir / pack_name
        shutil.copytree(src_ext, dst_stg)
        src_conv = converted_dir / pack_name
        overlaid = 0
        for dp, _, fns in os.walk(src_conv):
            for fn in fns:
                rel = pathlib.Path(dp, fn).relative_to(src_conv)
                target = dst_stg / rel
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(pathlib.Path(dp, fn), target)
                overlaid += 1
        out_prefix = str(out_pack_dir / pack_name)
        subprocess.check_call([str(tool_exe), "pack", str(dst_stg), out_prefix], stdout=subprocess.DEVNULL)
        subprocess.check_call([str(tool_exe), "verify", out_prefix, str(dst_stg)], stdout=subprocess.DEVNULL)
        counts[pack_name] = {
            "overlaid_py": overlaid,
            "index_sha256": sha256((out_pack_dir / f"{pack_name}.index").read_bytes()),
            "data_sha256": sha256((out_pack_dir / f"{pack_name}.data").read_bytes()),
        }
    return counts


def _atomic_write_bytes(dst: pathlib.Path, data: bytes):
    dst.parent.mkdir(parents=True, exist_ok=True)
    tmp = dst.with_name(dst.name + ".tmp_write")
    with open(tmp, "wb") as f:
        f.write(data)
        f.flush()
        os.fsync(f.fileno())
    os.replace(tmp, dst)


def _write_tx_journal(backup_dir: pathlib.Path, journal: dict):
    jpath = backup_dir / "pack_tx_journal.json"
    raw = json.dumps(journal, indent=2, sort_keys=True).encode("utf-8")
    _atomic_write_bytes(jpath, raw)


def _read_tx_journal(backup_dir: pathlib.Path):
    jpath = backup_dir / "pack_tx_journal.json"
    if not jpath.is_file():
        return None
    try:
        return json.loads(jpath.read_text(encoding="utf-8"))
    except Exception as exc:  # noqa: BLE001
        raise DeploymentSafetyError(f"Bozuk islem gunlugu (pack_tx_journal.json): {jpath}: {exc}") from exc


def validate_pack_set(dir_path: pathlib.Path, expected_hashes: dict, label: str) -> dict:
    if not dir_path.is_dir():
        raise DeploymentSafetyError(f"{label} dizini mevcut degil: {dir_path}")
    actual = {}
    for name in PACK_ARCHIVE_NAMES:
        p = dir_path / name
        if not p.is_file():
            raise DeploymentSafetyError(f"Eksik {label} dosyasi: {p}")
        h = sha256(p.read_bytes())
        actual[name] = h
        if expected_hashes is not None:
            exp = expected_hashes.get(name)
            if exp and h != exp:
                raise DeploymentSafetyError(
                    f"{label} SHA-256 uyusmazligi ({name}): beklenen={exp}, bulunan={h}"
                )
    return actual


def validate_existing_backup(backup_dir: pathlib.Path, expected_backup_hashes: dict) -> tuple:
    """Return (has_existing_backup: bool, backup_hashes: dict).
    Raises DeploymentSafetyError if backup is partial (1..3 files) or any file hash mismatches expected_backup_hashes.
    """
    if not backup_dir.exists():
        return False, {}
    present = [name for name in PACK_ARCHIVE_NAMES if (backup_dir / name).is_file()]
    if len(present) == 0:
        return False, {}
    if len(present) != len(PACK_ARCHIVE_NAMES):
        missing = [n for n in PACK_ARCHIVE_NAMES if n not in present]
        raise DeploymentSafetyError(
            f"Eksik/kismi yedek tespit edildi ({backup_dir}): mevcut={present}, eksik={missing}"
        )
    hashes = validate_pack_set(backup_dir, expected_backup_hashes, "Mevcut yedek (backup)")
    return True, hashes


def _cleanup_staging_temps(target_pack_dir: pathlib.Path):
    if not target_pack_dir.is_dir():
        return []
    removed = []
    for name in PACK_ARCHIVE_NAMES:
        for suffix in (".deploy_tmp", ".rollback_tmp", ".recover_tmp"):
            tp = target_pack_dir / (name + suffix)
            if tp.exists():
                try:
                    tp.unlink()
                    removed.append(tp.name)
                except OSError:
                    pass
    return removed


def recover_pack_transaction(
    target_pack_dir: pathlib.Path,
    backup_dir: pathlib.Path,
    expected_backup_hashes: dict = None,
) -> dict:
    """Interrupted veya basarisiz bir paket islemini dogrulanmis yedekten kurtarir.
    Dosya grubu islemleri isletim sistemi seviyesinde tek adimda atomik olmadigindan,
    islem gunlugu (pack_tx_journal.json) ve dogrulanmis yedek uzerinden deterministik kurtarma saglar.
    """
    enforce_cpython_3119_x64()
    if expected_backup_hashes is None:
        expected_backup_hashes = EXPECTED_ORIG_PACK_SHA256
    journal = _read_tx_journal(backup_dir)
    has_bak, bak_hashes = validate_existing_backup(backup_dir, expected_backup_hashes)
    if not has_bak:
        raise DeploymentSafetyError(f"Kurtarma basarisiz: gecerli tam yedek bulunamadi: {backup_dir}")

    cleaned_temps = _cleanup_staging_temps(target_pack_dir)
    restored = {}
    for name in PACK_ARCHIVE_NAMES:
        bak_path = backup_dir / name
        dst_path = target_pack_dir / name
        raw = bak_path.read_bytes()
        if sha256(raw) != bak_hashes[name]:
            raise DeploymentSafetyError(f"Kurtarma sirasinda yedek SHA-256 bozulmasi: {bak_path}")
        tmp_path = target_pack_dir / (name + ".recover_tmp")
        with open(tmp_path, "wb") as f:
            f.write(raw)
            f.flush()
            os.fsync(f.fileno())
        if sha256(tmp_path.read_bytes()) != bak_hashes[name]:
            raise DeploymentSafetyError(f"Kurtarma gecici dosya SHA-256 hatasi: {tmp_path}")
        os.replace(tmp_path, dst_path)
        restored_hash = sha256(dst_path.read_bytes())
        if restored_hash != bak_hashes[name]:
            raise DeploymentSafetyError(f"Kurtarma sonrasi hedef SHA-256 hatasi: {dst_path}")
        restored[name] = restored_hash

    new_journal = {
        "operation": "recover-packs",
        "previous_state": journal.get("state") if journal else "NONE",
        "state": "RECOVERED_FROM_INTERRUPTION",
        "cleaned_temp_files": cleaned_temps,
        "restored_sha256": restored,
        "timestamp_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    }
    _write_tx_journal(backup_dir, new_journal)
    return new_journal


def deploy_packs_into(
    src_out2: pathlib.Path,
    target_pack_dir: pathlib.Path,
    backup_dir: pathlib.Path,
    apply: bool = False,
    expected_src_hashes: dict = None,
    expected_target_hashes: dict = None,
    expected_backup_hashes: dict = None,
    fault_injector=None,
) -> dict:
    """Python 3.11 out2/{root,locale}.{index,data} arsivlerini dogrulanmis sekilde dagitir.
    - Dagitmadan once kaynak (out2), hedef (orig_pack) ve mevcut yedek (backup) SHA-256 degerlerini zorunlu dogrular.
    - Mevcut yedek eksik, bozuk veya farkli surumdeyse dagitimi hicbir dosyaya dokunmadan reddeder.
    - Tamamlanmamis bir islem gunlugu (IN_PROGRESS) varsa once kurtarma yapilmasini zorunlu kilar.
    - apply=False (dry-run) modunda hicbir dosya olusturmaz veya degistirmez.
    - apply=True modunda kismi kopyalama, disk/erisim hatasi veya kesinti olursa tum hedef dosyalari
      otomatik olarak dogrulanmis yedekten geri yukler ve SHA-256 esitligini kanitlar.
    """
    enforce_cpython_3119_x64()
    if expected_src_hashes is None:
        expected_src_hashes = EXPECTED_PY311_OUT2_SHA256
    if expected_target_hashes is None:
        expected_target_hashes = EXPECTED_ORIG_PACK_SHA256
    if expected_backup_hashes is None:
        expected_backup_hashes = EXPECTED_ORIG_PACK_SHA256

    existing_journal = _read_tx_journal(backup_dir)
    if existing_journal and existing_journal.get("state") in ("IN_PROGRESS", "FAILED_UNRECOVERED"):
        raise DeploymentSafetyError(
            f"Tamamlanmamis onceki paket islemi tespit edildi (state={existing_journal.get('state')}); "
            "yeni dagitimdan once recover-packs calistirilmalidir."
        )

    # Faz 1: Hicbir dosyaya yazmadan once tam SHA-256 on-kontrolu
    src_hashes = validate_pack_set(src_out2, expected_src_hashes, "Kaynak Python 3.11 (out2)")
    target_hashes = validate_pack_set(target_pack_dir, expected_target_hashes, "Hedef paket (target)")
    has_existing_backup, bak_hashes = validate_existing_backup(backup_dir, expected_backup_hashes)

    # Yedek mevcutsa hedef dosyalarla da birebir ayni orijinal surum oldugunu dogrula
    if has_existing_backup:
        for name in PACK_ARCHIVE_NAMES:
            if bak_hashes[name] != target_hashes[name]:
                raise DeploymentSafetyError(
                    f"Yedek ile hedef paket SHA-256 uyusmazligi ({name}): backup={bak_hashes[name]} target={target_hashes[name]}"
                )

    plan = {
        "apply": apply,
        "atomic_os_operation": False,
        "recovery_mechanism": "pack_tx_journal.json + verified_backup_auto_rollback",
        "backup_existed_before": has_existing_backup,
        "compatibility_contract": COMPATIBILITY_CONTRACT,
        "files": {},
    }
    for name in PACK_ARCHIVE_NAMES:
        plan["files"][name] = {
            "backup_sha256": bak_hashes.get(name, target_hashes[name]),
            "current_target_sha256": target_hashes[name],
            "new_py311_sha256": src_hashes[name],
        }

    if not apply:
        return plan

    # Faz 2: Yedek yoksa once tam yedegi olustur ve 4/4 SHA-256 dogrula (hedefe hala dokunulmadi)
    backup_dir.mkdir(parents=True, exist_ok=True)
    if not has_existing_backup:
        for name in PACK_ARCHIVE_NAMES:
            raw_dst = (target_pack_dir / name).read_bytes()
            if sha256(raw_dst) != expected_backup_hashes[name]:
                raise DeploymentSafetyError(f"Yedekleme oncesi hedef SHA-256 degisti: {name}")
            _atomic_write_bytes(backup_dir / name, raw_dst)
        _, bak_hashes = validate_existing_backup(backup_dir, expected_backup_hashes)

    # Faz 3: Islem gunlugunu IN_PROGRESS olarak kaydet
    tx_journal = {
        "operation": "deploy-packs",
        "state": "IN_PROGRESS",
        "pre_target_sha256": target_hashes,
        "backup_sha256": bak_hashes,
        "desired_sha256": src_hashes,
        "completed_files": [],
        "timestamp_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    }
    _write_tx_journal(backup_dir, tx_journal)

    # Faz 4: Gecici .deploy_tmp yazimi, fsync, SHA-256 dogrulama ve os.replace (hata aninda otomatik tam geri yukleme)
    try:
        for idx, name in enumerate(PACK_ARCHIVE_NAMES):
            src_path = src_out2 / name
            dst_path = target_pack_dir / name
            tmp_path = target_pack_dir / (name + ".deploy_tmp")
            raw_src = src_path.read_bytes()
            if sha256(raw_src) != src_hashes[name]:
                raise DeploymentSafetyError(f"Kaynak dosya okuma sirasinda bozuldu: {src_path}")

            if fault_injector is not None:
                fault_injector("before_stage", idx, name, dst_path, tmp_path)

            with open(tmp_path, "wb") as f:
                f.write(raw_src)
                f.flush()
                os.fsync(f.fileno())

            if fault_injector is not None:
                fault_injector("after_stage_before_replace", idx, name, dst_path, tmp_path)

            staged_hash = sha256(tmp_path.read_bytes())
            if staged_hash != src_hashes[name]:
                raise DeploymentSafetyError(
                    f"Gecici dagitim dosyasi (.deploy_tmp) SHA-256 bozulmasi ({name}): beklenen={src_hashes[name]}, bulunan={staged_hash}"
                )

            os.replace(tmp_path, dst_path)
            deployed_hash = sha256(dst_path.read_bytes())
            if deployed_hash != src_hashes[name]:
                raise DeploymentSafetyError(
                    f"Hedef dosya yazim sonrasi SHA-256 bozulmasi ({name}): beklenen={src_hashes[name]}, bulunan={deployed_hash}"
                )

            plan["files"][name]["deployed_target_sha256"] = deployed_hash
            tx_journal["completed_files"].append(name)
            _write_tx_journal(backup_dir, tx_journal)

            if fault_injector is not None:
                fault_injector("after_replace", idx, name, dst_path, tmp_path)

        tx_journal["state"] = "COMMITTED"
        _write_tx_journal(backup_dir, tx_journal)
        plan["transaction_state"] = "COMMITTED"
        return plan
    except Exception as exc:
        # Otomatik kurtarma: tum .deploy_tmp kalintilarini sil ve 4 dosyanin tamamini dogrulanmis yedekten geri yukle
        recovery_errors = []
        _cleanup_staging_temps(target_pack_dir)
        restored_hashes = {}
        for rname in PACK_ARCHIVE_NAMES:
            try:
                bak_bytes = (backup_dir / rname).read_bytes()
                if sha256(bak_bytes) != bak_hashes[rname]:
                    raise DeploymentSafetyError(f"Kurtarma yedegi bozuk: {rname}")
                rtmp = target_pack_dir / (rname + ".recover_tmp")
                with open(rtmp, "wb") as rf:
                    rf.write(bak_bytes)
                    rf.flush()
                    os.fsync(rf.fileno())
                os.replace(rtmp, target_pack_dir / rname)
                rh = sha256((target_pack_dir / rname).read_bytes())
                if rh != bak_hashes[rname]:
                    raise DeploymentSafetyError(f"Kurtarma sonrasi SHA-256 uyusmazligi: {rname}")
                restored_hashes[rname] = rh
            except Exception as rexc:  # noqa: BLE001
                recovery_errors.append(f"{rname}: {rexc}")

        recovered_ok = (len(recovery_errors) == 0 and restored_hashes == bak_hashes)
        tx_journal["state"] = "ROLLED_BACK_AFTER_FAILURE" if recovered_ok else "FAILED_UNRECOVERED"
        tx_journal["error"] = f"{type(exc).__name__}: {exc}"
        tx_journal["recovered_all_4_to_backup"] = recovered_ok
        tx_journal["restored_sha256"] = restored_hashes
        tx_journal["recovery_errors"] = recovery_errors
        _write_tx_journal(backup_dir, tx_journal)
        raise DeploymentSafetyError(
            f"Dagitim hatasi nedeniyle islem iptal edildi ve yedekten geri alindi "
            f"(recovered_all_4={recovered_ok}): {type(exc).__name__}: {exc}"
        ) from exc


def rollback_packs_into(
    target_pack_dir: pathlib.Path,
    backup_dir: pathlib.Path,
    apply: bool = False,
    expected_backup_hashes: dict = None,
    fault_injector=None,
) -> dict:
    """Yalnizca 4 EterPack arsivini (root.index, root.data, locale.index, locale.data)
    dogrulanmis orig_pack yedeginden geri yukler.
    ONEMLI: Bu islem client/files/lib dizinini GERI ALMAZ ve Python 2.7 orig_pack arsivlerine
    geri donus CPython 3.11.9 istemcisi icin calisan bir rollback DEGILDIR.
    """
    enforce_cpython_3119_x64()
    if expected_backup_hashes is None:
        expected_backup_hashes = EXPECTED_ORIG_PACK_SHA256

    has_bak, bak_hashes = validate_existing_backup(backup_dir, expected_backup_hashes)
    if not has_bak:
        raise DeploymentSafetyError(f"Rollback basarisiz: gecerli tam yedek bulunamadi: {backup_dir}")

    if not target_pack_dir.is_dir():
        raise DeploymentSafetyError(f"Hedef paket dizini mevcut degil: {target_pack_dir}")

    plan = {
        "apply": apply,
        "scope": "packs_only_4_archives",
        "archives_included": list(PACK_ARCHIVE_NAMES),
        "client_files_lib_modified": False,
        "restored_pack_format": "python27_orig_pack",
        "compatible_with_embedded_python311": False,
        "compatibility_contract": COMPATIBILITY_CONTRACT,
        "files": {},
    }
    for name in PACK_ARCHIVE_NAMES:
        dst = target_pack_dir / name
        plan["files"][name] = {
            "backup_sha256": bak_hashes[name],
            "before_rollback_sha256": sha256(dst.read_bytes()) if dst.is_file() else None,
        }

    if not apply:
        return plan

    tx_journal = {
        "operation": "rollback-packs",
        "state": "IN_PROGRESS",
        "backup_sha256": bak_hashes,
        "completed_files": [],
        "timestamp_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    }
    _write_tx_journal(backup_dir, tx_journal)

    try:
        _cleanup_staging_temps(target_pack_dir)
        for idx, name in enumerate(PACK_ARCHIVE_NAMES):
            bak_path = backup_dir / name
            dst_path = target_pack_dir / name
            tmp_path = target_pack_dir / (name + ".rollback_tmp")
            raw_bak = bak_path.read_bytes()
            if sha256(raw_bak) != bak_hashes[name]:
                raise DeploymentSafetyError(f"Yedek dosya okuma sirasinda bozuldu: {bak_path}")

            if fault_injector is not None:
                fault_injector("before_stage", idx, name, dst_path, tmp_path)

            with open(tmp_path, "wb") as f:
                f.write(raw_bak)
                f.flush()
                os.fsync(f.fileno())

            if fault_injector is not None:
                fault_injector("after_stage_before_replace", idx, name, dst_path, tmp_path)

            if sha256(tmp_path.read_bytes()) != bak_hashes[name]:
                raise DeploymentSafetyError(f"Gecici rollback dosyasi SHA-256 bozulmasi: {tmp_path}")

            os.replace(tmp_path, dst_path)
            restored_hash = sha256(dst_path.read_bytes())
            if restored_hash != bak_hashes[name]:
                raise DeploymentSafetyError(f"Rollback sonrasi hedef SHA-256 uyusmazligi: {dst_path}")

            plan["files"][name]["restored_target_sha256"] = restored_hash
            tx_journal["completed_files"].append(name)
            _write_tx_journal(backup_dir, tx_journal)

            if fault_injector is not None:
                fault_injector("after_replace", idx, name, dst_path, tmp_path)

        tx_journal["state"] = "COMMITTED"
        _write_tx_journal(backup_dir, tx_journal)
        plan["transaction_state"] = "COMMITTED"
        return plan
    except Exception as exc:
        _cleanup_staging_temps(target_pack_dir)
        tx_journal["state"] = "FAILED_UNRECOVERED"
        tx_journal["error"] = f"{type(exc).__name__}: {exc}"
        _write_tx_journal(backup_dir, tx_journal)
        raise DeploymentSafetyError(f"Rollback sirasinda hata olustu: {type(exc).__name__}: {exc}") from exc


def run_deployment_safety_tests(sandbox_dir: pathlib.Path, src_out2: pathlib.Path, orig_pack_dir: pathlib.Path) -> dict:
    """Tum negatif surum kilidi, SHA-256, bozuk/eksik yedek, kismi kopyalama, disk/erisim hatasi,
    islem kesintisi (crash journal) ve rollback kapsam testlerini YALNIZCA izole gecici dizinde calistirir.
    Gercek client/files/pack ve client/files/lib dizinlerine ASLA dokunmaz.
    """
    import shutil

    # Gercek uretim dizinlerinin test oncesi anlik goruntusu (SHA-256 + mtime_ns)
    real_pack_before = {
        name: (sha256((CLIENT_FILES / "pack" / name).read_bytes()), (CLIENT_FILES / "pack" / name).stat().st_mtime_ns)
        for name in PACK_ARCHIVE_NAMES
    }
    real_lib_manifest_before = sha256((CLIENT_FILES / "lib" / "MANIFEST.json").read_bytes())

    if sandbox_dir.exists():
        shutil.rmtree(sandbox_dir)
    sandbox_dir.mkdir(parents=True, exist_ok=True)

    results = {
        "version_lock_tests": {},
        "sha_and_backup_negative_tests": {},
        "fault_injection_and_recovery_tests": {},
        "rollback_scope_and_contract_tests": {},
        "real_client_files_untouched": False,
        "all_passed": False,
    }

    def fresh_env(case_name: str):
        case_dir = sandbox_dir / case_name
        if case_dir.exists():
            shutil.rmtree(case_dir)
        s_out2 = case_dir / "out2"
        t_pack = case_dir / "target_pack"
        b_pack = case_dir / "backup_pack"
        s_out2.mkdir(parents=True, exist_ok=True)
        t_pack.mkdir(parents=True, exist_ok=True)
        for name in PACK_ARCHIVE_NAMES:
            shutil.copy2(src_out2 / name, s_out2 / name)
            shutil.copy2(orig_pack_dir / name, t_pack / name)
        return case_dir, s_out2, t_pack, b_pack

    # ------------------------------------------------------------------
    # 1) Python Surum ve Mimari Kilidi Testleri (Pozitif + 6 Negatif + Subprocess)
    # ------------------------------------------------------------------
    v_ok = check_python_environment()
    results["version_lock_tests"]["current_cpython_3119_x64_pass"] = (
        v_ok["version"] == [3, 11, 9] and v_ok["implementation"] == "CPython" and v_ok["ptr_size"] == 8
    )

    neg_env_cases = {
        "reject_python_3_11_8": ((3, 11, 8), "CPython", 8, "AMD64", REQUIRED_MAGIC_HEX),
        "reject_python_3_12_0": ((3, 12, 0), "CPython", 8, "AMD64", "cb0d0d0a"),
        "reject_python_2_7_18": ((2, 7, 18), "CPython", 8, "AMD64", "03f30d0a"),
        "reject_non_cpython_pypy": ((3, 11, 9), "PyPy", 8, "AMD64", REQUIRED_MAGIC_HEX),
        "reject_32bit_ptr_size_4": ((3, 11, 9), "CPython", 4, "AMD64", REQUIRED_MAGIC_HEX),
        "reject_x86_i386_machine": ((3, 11, 9), "CPython", 8, "I386", REQUIRED_MAGIC_HEX),
        "reject_bad_pyc_magic": ((3, 11, 9), "CPython", 8, "AMD64", "550d0d0a"),
    }
    for cname, args in neg_env_cases.items():
        rejected = False
        try:
            check_python_environment(*args)
        except VersionLockError:
            rejected = True
        results["version_lock_tests"][cname] = rejected

    # Subprocess CLI testi: yanlis surum ve 32-bit mimari ile cagrildiginda rc == 2 ve 0 dosya degisimi
    for sim_label, sim_val in (
        ("subprocess_cli_reject_3_11_8", "3.11.8,CPython,8,AMD64,a70d0d0a"),
        ("subprocess_cli_reject_32bit_x86", "3.11.9,CPython,4,I386,a70d0d0a"),
    ):
        env_sim = dict(os.environ, MT2_SIMULATE_PYTHON_ENV=sim_val, PYTHONHASHSEED="0")
        proc = subprocess.run(
            [sys.executable, str(pathlib.Path(__file__).resolve()), "deploy-packs"],
            env=env_sim,
            capture_output=True,
            text=True,
        )
        results["version_lock_tests"][sim_label] = (proc.returncode == 2 and "FATAL:" in proc.stderr)

    # ------------------------------------------------------------------
    # 2) SHA-256 ve Yedek Dogrulamasi Negatif Testleri (8 Senaryo)
    # ------------------------------------------------------------------
    # 2a: Bozuk kaynak (out2/locale.data 1 bayt bozulmus)
    _, s_out2, t_pack, b_pack = fresh_env("neg_corrupt_src")
    raw_bad = bytearray((s_out2 / "locale.data").read_bytes())
    raw_bad[0] ^= 0xFF
    (s_out2 / "locale.data").write_bytes(bytes(raw_bad))
    try:
        deploy_packs_into(s_out2, t_pack, b_pack, apply=True)
        results["sha_and_backup_negative_tests"]["reject_corrupted_src_out2"] = False
    except DeploymentSafetyError:
        results["sha_and_backup_negative_tests"]["reject_corrupted_src_out2"] = (
            validate_pack_set(t_pack, EXPECTED_ORIG_PACK_SHA256, "target") == EXPECTED_ORIG_PACK_SHA256
            and not b_pack.exists()
        )

    # 2b: Eksik kaynak dosyasi (out2/root.index silinmis)
    _, s_out2, t_pack, b_pack = fresh_env("neg_missing_src")
    (s_out2 / "root.index").unlink()
    try:
        deploy_packs_into(s_out2, t_pack, b_pack, apply=True)
        results["sha_and_backup_negative_tests"]["reject_missing_src_out2"] = False
    except DeploymentSafetyError:
        results["sha_and_backup_negative_tests"]["reject_missing_src_out2"] = (
            validate_pack_set(t_pack, EXPECTED_ORIG_PACK_SHA256, "target") == EXPECTED_ORIG_PACK_SHA256
            and not b_pack.exists()
        )

    # 2c: Hedef dosya beklenen orijinal SHA-256'da degil (orn. 1 bayt degismis veya zaten out2)
    _, s_out2, t_pack, b_pack = fresh_env("neg_Unexpected_target")
    t_bad = bytearray((t_pack / "root.data").read_bytes())
    t_bad[10] ^= 0xFF
    expected_bad_target_hash = sha256(bytes(t_bad))
    (t_pack / "root.data").write_bytes(bytes(t_bad))
    try:
        deploy_packs_into(s_out2, t_pack, b_pack, apply=True)
        results["sha_and_backup_negative_tests"]["reject_unexpected_target_sha"] = False
    except DeploymentSafetyError:
        results["sha_and_backup_negative_tests"]["reject_unexpected_target_sha"] = (
            sha256((t_pack / "root.data").read_bytes()) == expected_bad_target_hash
            and not b_pack.exists()
        )

    # 2d: Kismi/eksik mevcut yedek (backup dizininde 4 dosyadan yalnizca 3'u var)
    _, s_out2, t_pack, b_pack = fresh_env("neg_partial_backup")
    b_pack.mkdir(parents=True, exist_ok=True)
    for name in ("root.index", "root.data", "locale.index"):
        shutil.copy2(t_pack / name, b_pack / name)
    try:
        deploy_packs_into(s_out2, t_pack, b_pack, apply=True)
        results["sha_and_backup_negative_tests"]["reject_partial_existing_backup"] = False
    except DeploymentSafetyError:
        results["sha_and_backup_negative_tests"]["reject_partial_existing_backup"] = (
            validate_pack_set(t_pack, EXPECTED_ORIG_PACK_SHA256, "target") == EXPECTED_ORIG_PACK_SHA256
        )

    # 2e: Bozuk mevcut yedek (backup/root.index 1 bayt bozulmus)
    _, s_out2, t_pack, b_pack = fresh_env("neg_corrupt_backup")
    b_pack.mkdir(parents=True, exist_ok=True)
    for name in PACK_ARCHIVE_NAMES:
        shutil.copy2(t_pack / name, b_pack / name)
    b_bad = bytearray((b_pack / "root.index").read_bytes())
    b_bad[0] ^= 0xFF
    (b_pack / "root.index").write_bytes(bytes(b_bad))
    try:
        deploy_packs_into(s_out2, t_pack, b_pack, apply=True)
        results["sha_and_backup_negative_tests"]["reject_corrupted_existing_backup"] = False
    except DeploymentSafetyError:
        results["sha_and_backup_negative_tests"]["reject_corrupted_existing_backup"] = (
            validate_pack_set(t_pack, EXPECTED_ORIG_PACK_SHA256, "target") == EXPECTED_ORIG_PACK_SHA256
        )

    # 2f: Yanlis surumde mevcut yedek (backup dizininde orig_pack yerine out2 dosyalari var)
    _, s_out2, t_pack, b_pack = fresh_env("neg_wrong_version_backup")
    b_pack.mkdir(parents=True, exist_ok=True)
    for name in PACK_ARCHIVE_NAMES:
        shutil.copy2(s_out2 / name, b_pack / name)
    try:
        deploy_packs_into(s_out2, t_pack, b_pack, apply=True)
        results["sha_and_backup_negative_tests"]["reject_wrong_version_backup"] = False
    except DeploymentSafetyError:
        results["sha_and_backup_negative_tests"]["reject_wrong_version_backup"] = (
            validate_pack_set(t_pack, EXPECTED_ORIG_PACK_SHA256, "target") == EXPECTED_ORIG_PACK_SHA256
        )

    # 2g: Rollback sirasinda bozuk/yanlis yedek reddi (hedef dosyalara dokunmadan hata verir)
    _, s_out2, t_pack, b_pack = fresh_env("neg_rollback_bad_backup")
    deploy_packs_into(s_out2, t_pack, b_pack, apply=True)
    rb_bad = bytearray((b_pack / "locale.data").read_bytes())
    rb_bad[0] ^= 0xFF
    (b_pack / "locale.data").write_bytes(bytes(rb_bad))
    try:
        rollback_packs_into(t_pack, b_pack, apply=True)
        results["sha_and_backup_negative_tests"]["reject_rollback_corrupted_backup"] = False
    except DeploymentSafetyError:
        results["sha_and_backup_negative_tests"]["reject_rollback_corrupted_backup"] = (
            validate_pack_set(t_pack, EXPECTED_PY311_OUT2_SHA256, "target_still_out2") == EXPECTED_PY311_OUT2_SHA256
        )

    # 2h: Dry-run (apply=False) izolasyonu: ne backup_dir olusturur ne target_pack_dir degistirir
    _, s_out2, t_pack, b_pack = fresh_env("neg_dry_run_isolation")
    dry_plan = deploy_packs_into(s_out2, t_pack, b_pack, apply=False)
    results["sha_and_backup_negative_tests"]["dry_run_zero_writes"] = (
        dry_plan["apply"] is False
        and not b_pack.exists()
        and validate_pack_set(t_pack, EXPECTED_ORIG_PACK_SHA256, "target") == EXPECTED_ORIG_PACK_SHA256
    )

    # ------------------------------------------------------------------
    # 3) Kismi Dagitim, Disk/Erisim Hatasi ve Kesinti Kurtarma Testleri (4 Senaryo)
    # ------------------------------------------------------------------
    # 3a: 3. dosyada (locale.index) disk dolu / IO hatasi (OSError ENOSPC) -> 1. ve 2. dosyalar otomatik geri alinir
    _, s_out2, t_pack, b_pack = fresh_env("fault_disk_error_3rd_file")

    def fault_disk_full(step, idx, name, dst_path, tmp_path):
        if step == "before_stage" and idx == 2:  # root.index ve root.data zaten degistirildi!
            raise OSError(28, "Simulated ENOSPC: No space left on device", str(tmp_path))

    try:
        deploy_packs_into(s_out2, t_pack, b_pack, apply=True, fault_injector=fault_disk_full)
        results["fault_injection_and_recovery_tests"]["disk_error_3rd_file_auto_rollback"] = False
    except DeploymentSafetyError:
        j_state = _read_tx_journal(b_pack)
        results["fault_injection_and_recovery_tests"]["disk_error_3rd_file_auto_rollback"] = (
            validate_pack_set(t_pack, EXPECTED_ORIG_PACK_SHA256, "recovered_target") == EXPECTED_ORIG_PACK_SHA256
            and j_state is not None
            and j_state.get("state") == "ROLLED_BACK_AFTER_FAILURE"
            and j_state.get("recovered_all_4_to_backup") is True
        )

    # 3b: 2. dosyada (root.data) erisim/izin hatasi (PermissionError) -> root.index (Py3.11) + root.data (Py2.7) uyumsuzlugu onlenir
    _, s_out2, t_pack, b_pack = fresh_env("fault_permission_error_2nd_file")

    def fault_perm_denied(step, idx, name, dst_path, tmp_path):
        if step == "after_stage_before_replace" and name == "root.data":
            raise PermissionError(13, "Simulated EACCES: Access is denied", str(dst_path))

    try:
        deploy_packs_into(s_out2, t_pack, b_pack, apply=True, fault_injector=fault_perm_denied)
        results["fault_injection_and_recovery_tests"]["permission_error_2nd_file_auto_rollback"] = False
    except DeploymentSafetyError:
        j_state = _read_tx_journal(b_pack)
        leftover_temps = list(t_pack.glob("*.*_tmp"))
        results["fault_injection_and_recovery_tests"]["permission_error_2nd_file_auto_rollback"] = (
            validate_pack_set(t_pack, EXPECTED_ORIG_PACK_SHA256, "recovered_target") == EXPECTED_ORIG_PACK_SHA256
            and len(leftover_temps) == 0
            and j_state is not None
            and j_state.get("state") == "ROLLED_BACK_AFTER_FAILURE"
        )

    # 3c: 4. dosyada (.deploy_tmp) kismi/bozuk yazim -> SHA-256 kontrolu yakalar ve ilk 3 dosyayi da geri alir
    _, s_out2, t_pack, b_pack = fresh_env("fault_corrupt_tmp_4th_file")

    def fault_corrupt_tmp(step, idx, name, dst_path, tmp_path):
        if step == "after_stage_before_replace" and name == "locale.data":
            corrupted = bytearray(tmp_path.read_bytes())
            corrupted[-1] ^= 0x55
            tmp_path.write_bytes(bytes(corrupted))

    try:
        deploy_packs_into(s_out2, t_pack, b_pack, apply=True, fault_injector=fault_corrupt_tmp)
        results["fault_injection_and_recovery_tests"]["corrupt_staged_file_auto_rollback"] = False
    except DeploymentSafetyError:
        results["fault_injection_and_recovery_tests"]["corrupt_staged_file_auto_rollback"] = (
            validate_pack_set(t_pack, EXPECTED_ORIG_PACK_SHA256, "recovered_target") == EXPECTED_ORIG_PACK_SHA256
        )

    # 3d: Sert surec kesintisi (SIGKILL / guc kesintisi) simulasyonu:
    # 2 dosya (root.index, root.data) out2 ile degistirilmis, locale.index.deploy_tmp yarim kalmis ve journal state=IN_PROGRESS
    _, s_out2, t_pack, b_pack = fresh_env("fault_hard_kill_journal_recovery")
    b_pack.mkdir(parents=True, exist_ok=True)
    for name in PACK_ARCHIVE_NAMES:
        shutil.copy2(t_pack / name, b_pack / name)
    for name in ("root.index", "root.data"):
        shutil.copy2(s_out2 / name, t_pack / name)
    (t_pack / "locale.index.deploy_tmp").write_bytes(b"partial_interrupted_bytes")
    _write_tx_journal(
        b_pack,
        {
            "operation": "deploy-packs",
            "state": "IN_PROGRESS",
            "completed_files": ["root.index", "root.data"],
        },
    )
    # Tamamlanmamis journal varken yeni deploy reddedilmeli
    blocked_when_in_progress = False
    try:
        deploy_packs_into(s_out2, t_pack, b_pack, apply=True)
    except DeploymentSafetyError:
        blocked_when_in_progress = True

    rec_res = recover_pack_transaction(t_pack, b_pack, EXPECTED_ORIG_PACK_SHA256)
    results["fault_injection_and_recovery_tests"]["hard_interruption_journal_block_and_recover"] = (
        blocked_when_in_progress
        and rec_res.get("state") == "RECOVERED_FROM_INTERRUPTION"
        and "locale.index.deploy_tmp" in rec_res.get("cleaned_temp_files", [])
        and not (t_pack / "locale.index.deploy_tmp").exists()
        and validate_pack_set(t_pack, EXPECTED_ORIG_PACK_SHA256, "recovered_target") == EXPECTED_ORIG_PACK_SHA256
    )

    # ------------------------------------------------------------------
    # 4) Rollback Gercek Kapsami ve Uyumluluk Sozlesmesi Testi
    # ------------------------------------------------------------------
    _, s_out2, t_pack, b_pack = fresh_env("rollback_scope_verify")
    dep_plan = deploy_packs_into(s_out2, t_pack, b_pack, apply=True)
    rb_plan = rollback_packs_into(t_pack, b_pack, apply=True)
    results["rollback_scope_and_contract_tests"] = {
        "deploy_verified_all_4_out2": (
            validate_pack_set(t_pack, EXPECTED_PY311_OUT2_SHA256, "after_rollback_no")
            if False
            else all(dep_plan["files"][n]["deployed_target_sha256"] == EXPECTED_PY311_OUT2_SHA256[n] for n in PACK_ARCHIVE_NAMES)
        ),
        "rollback_restored_all_4_orig_pack": (
            validate_pack_set(t_pack, EXPECTED_ORIG_PACK_SHA256, "after_rollback") == EXPECTED_ORIG_PACK_SHA256
        ),
        "rollback_scope_is_packs_only_4_archives": rb_plan.get("scope") == "packs_only_4_archives",
        "rollback_does_not_modify_client_files_lib": rb_plan.get("client_files_lib_modified") is False,
        "rollback_not_marked_compatible_with_py311_client": rb_plan.get("compatible_with_embedded_python311") is False,
    }

    # Gercek client/files/pack ve client/files/lib dosyalarinin hic degismedigini (SHA-256 + mtime_ns) dogrula
    real_pack_after = {
        name: (sha256((CLIENT_FILES / "pack" / name).read_bytes()), (CLIENT_FILES / "pack" / name).stat().st_mtime_ns)
        for name in PACK_ARCHIVE_NAMES
    }
    real_lib_manifest_after = sha256((CLIENT_FILES / "lib" / "MANIFEST.json").read_bytes())
    results["real_client_files_untouched"] = (
        real_pack_before == real_pack_after
        and all(real_pack_after[n][0] == EXPECTED_ORIG_PACK_SHA256[n] for n in PACK_ARCHIVE_NAMES)
        and real_lib_manifest_before == real_lib_manifest_after
    )

    results["all_passed"] = (
        all(results["version_lock_tests"].values())
        and all(results["sha_and_backup_negative_tests"].values())
        and all(results["fault_injection_and_recovery_tests"].values())
        and all(results["rollback_scope_and_contract_tests"].values())
        and results["real_client_files_untouched"]
    )
    return results


def cmd_build_lib(a):
    enforce_cpython_3119_x64()
    _ensure_hashseed_zero()
    out_dir = pathlib.Path(a.out_dir) if getattr(a, "out_dir", None) else (WORK / "lib_rebuilt")
    if out_dir.resolve() == (CLIENT_FILES / "lib").resolve() and not getattr(a, "allow_client_lib_overwrite", False):
        raise DeploymentSafetyError(
            "Gercek client/files/lib dizinini korumak icin --allow-client-lib-overwrite olmadan uzerine yazilamaz."
        )
    tpl_path = CLIENT_FILES / "lib" / "MANIFEST.json"
    res = build_lib_into(out_dir, tpl_path)
    print("BUILD_LIB pyc=%d pyd=%d total=%d hashseed=0 out=%s" % (res["pyc"], res["pyd"], res["total"], out_dir))
    return 0


def cmd_stage_repack(a):
    enforce_cpython_3119_x64()
    counts = stage_and_repack_into(WORK / "extracted", WORK / "converted", WORK / "staging", WORK / "out2")
    print("STAGE_AND_REPACK", json.dumps(counts))
    return 0


def cmd_verify_repro(a):
    import shutil
    env_info = enforce_cpython_3119_x64()
    _ensure_hashseed_zero()
    tool_exe = WORK / "bin" / "eterpack_py_tool.exe"
    repro_dir = WORK / "repro_verify"
    if repro_dir.exists():
        shutil.rmtree(repro_dir)
    repro_dir.mkdir(parents=True, exist_ok=True)
    try:
        # 1) Orijinal client/files/pack/{root,locale}.{index,data} ile orig_pack yedeklerinin bayt-bayt esitligini dogrula
        orig_pack_hashes = {}
        orig_pack_intact = True
        for name in PACK_ARCHIVE_NAMES:
            h_client = sha256((CLIENT_FILES / "pack" / name).read_bytes())
            h_backup = sha256((WORK / "orig_pack" / name).read_bytes())
            exp_orig = EXPECTED_ORIG_PACK_SHA256[name]
            orig_pack_hashes[name] = {
                "client_pack_sha256": h_client,
                "orig_pack_backup_sha256": h_backup,
                "expected_orig_sha256": exp_orig,
                "identical": (h_client == h_backup == exp_orig),
            }
            if not (h_client == h_backup == exp_orig):
                orig_pack_intact = False

        # 2) Temiz gecici dizinde orig_pack'ten root ve locale paketlerini cikar ve extracted/ ile karsilastir
        repro_ext = repro_dir / "extracted"
        ext_counts = {}
        ext_match = True
        for pack_name in ("root", "locale"):
            out_sub = repro_ext / pack_name
            out_sub.mkdir(parents=True, exist_ok=True)
            subprocess.check_call([str(tool_exe), "extract", str(WORK / "orig_pack" / pack_name), str(out_sub)], stdout=subprocess.DEVNULL)
            n_files = 0
            for dp, _, fns in os.walk(out_sub):
                for fn in fns:
                    n_files += 1
                    rel = pathlib.Path(dp, fn).relative_to(out_sub)
                    if pathlib.Path(dp, fn).read_bytes() != (WORK / "extracted" / pack_name / rel).read_bytes():
                        ext_match = False
            ext_counts[pack_name] = n_files

        # 3) Temiz gecici dizinde 270 paket .py dosyasini bastan donustur ve converted/ ile SHA-256 karsilastir
        repro_conv = repro_dir / "converted"
        tool = make_tool()
        conv_total = 0
        conv_match = 0
        for area in AREAS:
            for rel, p in iter_py(area, repro_ext):
                conv_total += 1
                raw = p.read_bytes()
                text, _enc = decode_source(raw)
                out, _nl, _applied, _dstats = convert_text(tool, text, rel)
                out_bytes = out.encode("utf-8")
                dst = repro_conv / area / rel
                dst.parent.mkdir(parents=True, exist_ok=True)
                dst.write_bytes(out_bytes)
                if out_bytes == (WORK / "converted" / area / rel).read_bytes():
                    conv_match += 1

        # 4) Temiz gecici dizinde staging + EterPack pack + verify calistir ve out2/ ile karsilastir
        repro_stg = repro_dir / "staging"
        repro_out2 = repro_dir / "out2"
        pack_res = stage_and_repack_into(repro_ext, repro_conv, repro_stg, repro_out2)
        out2_match = True
        for pack_name in ("root", "locale"):
            rc_ver = subprocess.call([str(tool_exe), "verify", str(WORK / "out2" / pack_name), str(repro_stg / pack_name)], stdout=subprocess.DEVNULL)
            pack_res[pack_name]["out2_verified_against_repro_staging"] = (rc_ver == 0)
            if rc_ver != 0:
                out2_match = False
            for ext in ("index", "data"):
                aname = f"{pack_name}.{ext}"
                h_repro = sha256((repro_out2 / aname).read_bytes())
                h_out2 = sha256((WORK / "out2" / aname).read_bytes())
                exp_out2 = EXPECTED_PY311_OUT2_SHA256[aname]
                pack_res[pack_name][f"archive_sha256_identical_{ext}"] = (h_repro == h_out2 == exp_out2)
                pack_res[pack_name][f"out2_{ext}_sha256"] = h_out2
                if not (h_repro == h_out2 == exp_out2):
                    out2_match = False

        # 5) Temiz gecici dizinde 278 dosyalik lib'i bastan derle ve client/files/lib ile SHA-256 karsilastir
        repro_lib = repro_dir / "lib"
        lib_res = build_lib_into(repro_lib, CLIENT_FILES / "lib" / "MANIFEST.json")
        lib_sha_match = 0
        for rel, meta in lib_res["manifest"]["files"].items():
            cur_bytes = (CLIENT_FILES / "lib" / rel).read_bytes()
            if sha256(cur_bytes) == meta["sha256"]:
                lib_sha_match += 1

        # 6) Izole gecici dizinde tam dagitim guvenligi, negatif testler ve hata enjeksiyonu testlerini calistir
        safety_results = run_deployment_safety_tests(repro_dir / "safety_tests", repro_out2, WORK / "orig_pack")
        deployed_ok = safety_results["rollback_scope_and_contract_tests"]["deploy_verified_all_4_out2"]
        rollback_ok = safety_results["rollback_scope_and_contract_tests"]["rollback_restores_orig_pack"] if "rollback_restores_orig_pack" in safety_results["rollback_scope_and_contract_tests"] else safety_results["rollback_scope_and_contract_tests"]["rollback_restored_all_4_orig_pack"]

        all_verified = (
            orig_pack_intact
            and ext_match
            and conv_match == conv_total
            and out2_match
            and lib_sha_match == lib_res["total"]
            and safety_results["all_passed"]
        )

        report = {
            "python_environment_lock": env_info,
            "clean_git_clone_status": "BLOCKED (client/files/** ve build/** .gitignore kapsamindadir; Git deposu yalnizca kaynak kod ve araclari icerir)",
            "workspace_with_original_inputs_status": "VERIFIED" if all_verified else "FAIL",
            "compatibility_contract": COMPATIBILITY_CONTRACT,
            "orig_pack_matches_client_files_pack": orig_pack_intact,
            "orig_pack_hashes": orig_pack_hashes,
            "extracted_repro": {"counts": ext_counts, "all_923_identical": ext_match},
            "converted_repro": {"total": conv_total, "sha256_identical": conv_match},
            "pack_out2_repro": {"packs": pack_res, "all_4_archives_identical": out2_match},
            "lib_repro": {"total": lib_res["total"], "pyc": lib_res["pyc"], "pyd": lib_res["pyd"], "sha256_identical": lib_sha_match},
            "deploy_and_rollback_simulation": {
                "deployed_matches_out2": deployed_ok,
                "rollback_restores_orig_pack_4_archives": rollback_ok,
                "rollback_modifies_client_files_lib": False,
                "rollback_is_working_py311_client_state": False,
            },
            "deployment_safety_and_fault_injection_tests": safety_results,
        }
        (WORK / "manifest" / "reproducibility_and_deployment.json").write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
        print(
            "REPRO_VERIFY status=%s orig_pack_intact=%s extracted_923=%s converted=%d/%d out2_4_identical=%s lib=%d/%d deploy_rollback=%s safety_tests=%s"
            % (
                report["workspace_with_original_inputs_status"],
                orig_pack_intact,
                ext_match,
                conv_match,
                conv_total,
                out2_match,
                lib_sha_match,
                lib_res["total"],
                deployed_ok and rollback_ok,
                safety_results["all_passed"],
            )
        )
        return 0 if all_verified else 1
    finally:
        if repro_dir.exists():
            shutil.rmtree(repro_dir)


def cmd_verify_safety(a):
    enforce_cpython_3119_x64()
    _ensure_hashseed_zero()
    import shutil
    sandbox = WORK / "safety_verify_tmp"
    try:
        res = run_deployment_safety_tests(sandbox, WORK / "out2", WORK / "orig_pack")
        print(
            "DEPLOYMENT_SAFETY_TESTS all_passed=%s version_lock=%d/%d sha_backup=%d/%d fault_recovery=%d/%d rollback_scope=%d/%d real_files_untouched=%s"
            % (
                res["all_passed"],
                sum(res["version_lock_tests"].values()),
                len(res["version_lock_tests"]),
                sum(res["sha_and_backup_negative_tests"].values()),
                len(res["sha_and_backup_negative_tests"]),
                sum(res["fault_injection_and_recovery_tests"].values()),
                len(res["fault_injection_and_recovery_tests"]),
                sum(res["rollback_scope_and_contract_tests"].values()),
                len(res["rollback_scope_and_contract_tests"]),
                res["real_client_files_untouched"],
            )
        )
        return 0 if res["all_passed"] else 1
    finally:
        if sandbox.exists():
            shutil.rmtree(sandbox)


def cmd_deploy_packs(a):
    enforce_cpython_3119_x64()
    plan = deploy_packs_into(WORK / "out2", CLIENT_FILES / "pack", WORK / "orig_pack", apply=a.apply)
    print("DEPLOY_PACKS apply=%s" % a.apply, json.dumps(plan, indent=2))
    return 0


def cmd_rollback_packs(a):
    enforce_cpython_3119_x64()
    plan = rollback_packs_into(CLIENT_FILES / "pack", WORK / "orig_pack", apply=a.apply)
    print("ROLLBACK_PACKS apply=%s" % a.apply, json.dumps(plan, indent=2))
    return 0


def cmd_recover_packs(a):
    enforce_cpython_3119_x64()
    res = recover_pack_transaction(CLIENT_FILES / "pack", WORK / "orig_pack")
    print("RECOVER_PACKS", json.dumps(res, indent=2))
    return 0


def main():
    # Zorunlu CPython 3.11.9 x64 kilidi: arguman isleme veya dosya erisiminden once calisir
    enforce_cpython_3119_x64()
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "cmd",
        choices=[
            "inventory",
            "convert",
            "verify-compile",
            "build-lib",
            "stage-and-repack",
            "verify-reproducibility",
            "verify-deployment-safety",
            "deploy-packs",
            "rollback-packs",
            "recover-packs",
        ],
    )
    ap.add_argument("--apply", action="store_true", help="deploy-packs / rollback-packs icin gercek kopyalama uygula (varsayilan: dry-run)")
    ap.add_argument("--out-dir", help="build-lib hedef dizini (varsayilan: build/python311_migration/lib_rebuilt)")
    ap.add_argument("--allow-client-lib-overwrite", action="store_true", help="build-lib hedefi client/files/lib ise uzerine yazmaya izin ver")
    a = ap.parse_args()
    dispatch = {
        "inventory": cmd_inventory,
        "convert": cmd_convert,
        "verify-compile": cmd_compile,
        "build-lib": cmd_build_lib,
        "stage-and-repack": cmd_stage_repack,
        "verify-reproducibility": cmd_verify_repro,
        "verify-deployment-safety": cmd_verify_safety,
        "deploy-packs": cmd_deploy_packs,
        "rollback-packs": cmd_rollback_packs,
        "recover-packs": cmd_recover_packs,
    }
    return dispatch[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
