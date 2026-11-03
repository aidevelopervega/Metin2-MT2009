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
import json
import pathlib
import re
import sys
import warnings

warnings.simplefilter("ignore")
from lib2to3 import refactor  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
WORK = ROOT / "build" / "python311_migration"
AREAS = ("root", "locale")

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
    ],
    "uiquest.py": [("list(map(apply,z))", "[fn() for fn in z]", 1)],
    "ui.py": [
        ("\t\timport exceptions\n", "", 1),
        ("int(round(y))", "(int(y + 0.5) if y >= 0 else int(y - 0.5))", 1),  # Python 2 round(): .5 sifirdan uzaga
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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["inventory", "convert", "verify-compile"])
    a = ap.parse_args()
    if sys.version_info[:2] != (3, 11):
        print("UYARI: CPython 3.11 bekleniyor, bulunan", sys.version)
    return {"inventory": cmd_inventory, "convert": cmd_convert, "verify-compile": cmd_compile}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
