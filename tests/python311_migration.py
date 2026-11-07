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
import os
import pathlib
import re
import subprocess
import sys
import warnings

warnings.simplefilter("ignore")
from lib2to3 import refactor  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
WORK = ROOT / "build" / "python311_migration"
CLIENT_FILES = ROOT / "client" / "files"
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


def deploy_packs_into(src_out2: pathlib.Path, target_pack_dir: pathlib.Path, backup_dir: pathlib.Path, apply: bool = False) -> dict:
    import shutil
    plan = {"apply": apply, "files": {}}
    backup_dir.mkdir(parents=True, exist_ok=True)
    for name in ("root.index", "root.data", "locale.index", "locale.data"):
        src = src_out2 / name
        dst = target_pack_dir / name
        bak = backup_dir / name
        if not src.is_file():
            raise FileNotFoundError(f"Eksik Python 3.11 paket ciktisi: {src}")
        if not dst.is_file():
            raise FileNotFoundError(f"Eksik hedef paket dosyasi: {dst}")
        if not bak.is_file():
            shutil.copy2(dst, bak)
        plan["files"][name] = {
            "backup_sha256": sha256(bak.read_bytes()),
            "current_target_sha256": sha256(dst.read_bytes()),
            "new_py311_sha256": sha256(src.read_bytes()),
        }
        if apply:
            shutil.copy2(src, dst)
            plan["files"][name]["deployed_target_sha256"] = sha256(dst.read_bytes())
    return plan


def rollback_packs_into(target_pack_dir: pathlib.Path, backup_dir: pathlib.Path, apply: bool = False) -> dict:
    import shutil
    plan = {"apply": apply, "files": {}}
    for name in ("root.index", "root.data", "locale.index", "locale.data"):
        bak = backup_dir / name
        dst = target_pack_dir / name
        if not bak.is_file():
            raise FileNotFoundError(f"Eksik yedek paket dosyasi: {bak}")
        plan["files"][name] = {
            "backup_sha256": sha256(bak.read_bytes()),
            "before_rollback_sha256": sha256(dst.read_bytes()) if dst.is_file() else None,
        }
        if apply:
            shutil.copy2(bak, dst)
            plan["files"][name]["restored_target_sha256"] = sha256(dst.read_bytes())
    return plan


def cmd_build_lib(a):
    _ensure_hashseed_zero()
    tpl_path = CLIENT_FILES / "lib" / "MANIFEST.json"
    res = build_lib_into(CLIENT_FILES / "lib", tpl_path)
    print("BUILD_LIB pyc=%d pyd=%d total=%d hashseed=0" % (res["pyc"], res["pyd"], res["total"]))
    return 0


def cmd_stage_repack(a):
    counts = stage_and_repack_into(WORK / "extracted", WORK / "converted", WORK / "staging", WORK / "out2")
    print("STAGE_AND_REPACK", json.dumps(counts))
    return 0


def cmd_verify_repro(a):
    import shutil
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
        for name in ("root.index", "root.data", "locale.index", "locale.data"):
            h_client = sha256((CLIENT_FILES / "pack" / name).read_bytes())
            h_backup = sha256((WORK / "orig_pack" / name).read_bytes())
            orig_pack_hashes[name] = {"client_pack_sha256": h_client, "orig_pack_backup_sha256": h_backup, "identical": h_client == h_backup}
            if h_client != h_backup:
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
            # out2 arsivinin de yeni uretilen repro_stg/pack_name ile 100% bayt-bayt eslestigini dogrula
            rc_ver = subprocess.call([str(tool_exe), "verify", str(WORK / "out2" / pack_name), str(repro_stg / pack_name)], stdout=subprocess.DEVNULL)
            pack_res[pack_name]["out2_verified_against_repro_staging"] = (rc_ver == 0)
            if rc_ver != 0:
                out2_match = False
            for ext in ("index", "data"):
                h_repro = sha256((repro_out2 / f"{pack_name}.{ext}").read_bytes())
                h_out2 = sha256((WORK / "out2" / f"{pack_name}.{ext}").read_bytes())
                pack_res[pack_name][f"archive_sha256_identical_{ext}"] = (h_repro == h_out2)
                pack_res[pack_name][f"out2_{ext}_sha256"] = h_out2
                if h_repro != h_out2:
                    out2_match = False

        # 5) Temiz gecici dizinde 278 dosyalik lib'i bastan derle ve client/files/lib ile SHA-256 karsilastir
        repro_lib = repro_dir / "lib"
        lib_res = build_lib_into(repro_lib, CLIENT_FILES / "lib" / "MANIFEST.json")
        lib_sha_match = 0
        for rel, meta in lib_res["manifest"]["files"].items():
            cur_bytes = (CLIENT_FILES / "lib" / rel).read_bytes()
            if sha256(cur_bytes) == meta["sha256"]:
                lib_sha_match += 1

        # 6) Izole hedef dizinde deploy + rollback dongusunu calistir ve 0 veri kaybi ile geri donusu kanitla
        mock_pack = repro_dir / "mock_pack"
        mock_bak = repro_dir / "mock_backup"
        mock_pack.mkdir(parents=True, exist_ok=True)
        for name in ("root.index", "root.data", "locale.index", "locale.data"):
            shutil.copy2(WORK / "orig_pack" / name, mock_pack / name)
        deploy_plan = deploy_packs_into(repro_out2, mock_pack, mock_bak, apply=True)
        deployed_ok = all(v["deployed_target_sha256"] == v["new_py311_sha256"] for v in deploy_plan["files"].values())
        rollback_plan = rollback_packs_into(mock_pack, mock_bak, apply=True)
        rollback_ok = all(v["restored_target_sha256"] == v["backup_sha256"] for v in rollback_plan["files"].values())

        report = {
            "clean_git_clone_status": "BLOCKED (client/files/** ve build/** .gitignore kapsamindadir; Git deposu yalnizca kaynak kod ve araclari icerir)",
            "workspace_with_original_inputs_status": "VERIFIED" if (orig_pack_intact and ext_match and conv_match == conv_total and out2_match and lib_sha_match == lib_res["total"] and deployed_ok and rollback_ok) else "FAIL",
            "orig_pack_matches_client_files_pack": orig_pack_intact,
            "orig_pack_hashes": orig_pack_hashes,
            "extracted_repro": {"counts": ext_counts, "all_923_identical": ext_match},
            "converted_repro": {"total": conv_total, "sha256_identical": conv_match},
            "pack_out2_repro": {"packs": pack_res, "all_4_archives_identical": out2_match},
            "lib_repro": {"total": lib_res["total"], "pyc": lib_res["pyc"], "pyd": lib_res["pyd"], "sha256_identical": lib_sha_match},
            "deploy_and_rollback_simulation": {"deployed_matches_out2": deployed_ok, "rollback_restores_orig_pack": rollback_ok},
        }
        (WORK / "manifest" / "reproducibility_and_deployment.json").write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
        print("REPRO_VERIFY status=%s orig_pack_intact=%s extracted_923=%s converted=%d/%d out2_4_identical=%s lib=%d/%d deploy_rollback=%s" % (
            report["workspace_with_original_inputs_status"], orig_pack_intact, ext_match, conv_match, conv_total,
            out2_match, lib_sha_match, lib_res["total"], deployed_ok and rollback_ok))
        return 0 if report["workspace_with_original_inputs_status"] == "VERIFIED" else 1
    finally:
        if repro_dir.exists():
            shutil.rmtree(repro_dir)


def cmd_deploy_packs(a):
    plan = deploy_packs_into(WORK / "out2", CLIENT_FILES / "pack", WORK / "orig_pack", apply=a.apply)
    print("DEPLOY_PACKS apply=%s" % a.apply, json.dumps(plan, indent=2))
    return 0


def cmd_rollback_packs(a):
    plan = rollback_packs_into(CLIENT_FILES / "pack", WORK / "orig_pack", apply=a.apply)
    print("ROLLBACK_PACKS apply=%s" % a.apply, json.dumps(plan, indent=2))
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["inventory", "convert", "verify-compile", "build-lib", "stage-and-repack", "verify-reproducibility", "deploy-packs", "rollback-packs"])
    ap.add_argument("--apply", action="store_true", help="deploy-packs / rollback-packs icin gercek kopyalama uygula (varsayilan: dry-run)")
    a = ap.parse_args()
    if sys.version_info[:2] != (3, 11):
        print("UYARI: CPython 3.11 bekleniyor, bulunan", sys.version)
    dispatch = {
        "inventory": cmd_inventory,
        "convert": cmd_convert,
        "verify-compile": cmd_compile,
        "build-lib": cmd_build_lib,
        "stage-and-repack": cmd_stage_repack,
        "verify-reproducibility": cmd_verify_repro,
        "deploy-packs": cmd_deploy_packs,
        "rollback-packs": cmd_rollback_packs,
    }
    return dispatch[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
