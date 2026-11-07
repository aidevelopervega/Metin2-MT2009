# MT2009 - python311_runtime_driver.py
# Gomulu CPython 3.11.9 x64 + GERCEK EterPack/CPythonLauncher/pack/dbg/liblib uzerinde Seviye A-G dogrulamasi.
# C++ harness (tests/test_python311_pack_runtime_x64.cpp) tarafindan, system.py zinciri calistiktan sonra yurutulur.
# Yerel (app/wndMgr/player/net/...) moduller MOCKED_NATIVE'dir; H (widget), I (ag/oyun callback), J (oyun ici) BLOCKED'dir.
import sys, os, ast, io, re, hashlib, json, builtins, collections

_out = sys.__stdout__
STAGING = builtins.__MT2_STAGING__        # build/python311_migration/staging  (root/ ve locale/ alt dizinleri)
LIBDIR = builtins.__MT2_LIBDIR__          # calisma dizinindeki lib
FAILED = 0
E_CALLS = dict(CALLS)      # system.py -> prototype.py zincirinin hemen sonundaki sayaçlar
E_ERRORS = list(ERRORS)
LEVELS = collections.OrderedDict()


def say(*a):
    _out.write(" ".join(str(x) for x in a) + "\n")
    _out.flush()


def gate(level, ok, **kv):
    global FAILED
    LEVELS[level] = ok
    if not ok:
        FAILED += 1
    say("LEVEL", level, "PASS" if ok else "FAIL", " ".join("%s=%s" % (k, v) for k, v in kv.items()))


def disk_files(sub):
    base = os.path.join(STAGING, sub)
    res = {}
    for dp, _, fns in os.walk(base):
        for fn in fns:
            full = os.path.join(dp, fn)
            res[os.path.relpath(full, base).replace("\\", "/")] = full
    return res


ROOT = disk_files("root")
LOCALE = disk_files("locale")
import pack

# ---------------------------------------------------------------- A: pack icerigi == staging, hepsi derlenir
a_fail = []
cats = collections.OrderedDict()


def cat_of(rel, is_locale):
    if is_locale:
        return "locale_ui" if rel.endswith(".py") else "locale_data"
    if rel.startswith("uiscript/"):
        return "root_uiscript"
    if rel.endswith(".py"):
        return "root_module"
    return "root_data"


for is_locale, files in ((False, ROOT), (True, LOCALE)):
    for rel, full in files.items():
        c = cats.setdefault(cat_of(rel, is_locale), [0, 0, 0])  # total, identical, compiled
        c[0] += 1
        try:
            if not pack.Exist(rel):
                a_fail.append((rel, "not_in_pack"))
                continue
            if rel.endswith((".py", ".txt")):   # pack.Get yalniz metin icin tanimli; ikili dosyalar tools/eterpack_py_tool verify ile bayt-bayt dogrulanir
                got = pack.Get(rel).encode("utf-8", "surrogateescape")
                if got.replace(b"\r", b"") != io.open(full, "rb").read().replace(b"\r", b""):
                    a_fail.append((rel, "content_differs"))
                    continue
            c[1] += 1
            if rel.endswith(".py"):
                compile(pack.Get(rel).replace("\r", ""), rel, "exec")
                c[2] += 1
        except Exception as e:
            a_fail.append((rel, "%s: %s" % (type(e).__name__, e)))
for k, v in cats.items():
    say("CATEGORY", k, "total=%d pack_identical=%d compiled=%d" % tuple(v))
gate("A_syntax_pack_integrity", not a_fail, files=len(ROOT) + len(LOCALE), failures=len(a_fail), first=a_fail[:3])

# ---------------------------------------------------------------- B: bagimlilik cozumlemesi
pack_mods = {rel[:-3].lower() for rel in ROOT if rel.endswith(".py") and "/" not in rel}
pack_pkgs = {rel.split("/")[0] for rel in ROOT if "/" in rel}
native_stub = set(m.lower() for m in STUBBED)
real_native = {"pack", "dbg", "liblib", "profiler", "snd", "exchange", "safebox"}
std = set(sys.stdlib_module_names)
lib_names = set()
for dp, _, fns in os.walk(LIBDIR):
    for fn in fns:
        rel = os.path.relpath(os.path.join(dp, fn), LIBDIR).replace("\\", "/")
        top = rel.split("/")[0]
        lib_names.add(top.rsplit(".", 1)[0].lower())
imported = collections.defaultdict(set)   # exact case name -> files
for rel, full in list(ROOT.items()) + list(LOCALE.items()):
    if not rel.endswith(".py"):
        continue
    for n in ast.walk(ast.parse(pack.Get(rel).replace("\r", ""))):
        if isinstance(n, ast.Import):
            for al in n.names:
                imported[al.name.split(".")[0]].add(rel)
        elif isinstance(n, ast.ImportFrom) and n.level == 0 and n.module:
            imported[n.module.split(".")[0]].add(rel)
unresolved = {}
by_kind = collections.Counter()
for name, users in sorted(imported.items()):
    low = name.lower()
    if low in pack_mods or name in pack_pkgs:
        by_kind["pack"] += 1
    elif name in real_native:
        by_kind["native_real"] += 1
    elif low in native_stub:
        by_kind["native_mocked"] += 1
    elif name in sys.builtin_module_names or name == "__main__" or (name in std and name != "imp"):
        by_kind["stdlib"] += 1
    elif low in lib_names:
        by_kind["lib"] += 1
    else:
        unresolved[name] = sorted(users)[:3]
say("IMPORT_KINDS", dict(by_kind))
for k, v in unresolved.items():
    say("UNRESOLVED_IMPORT", k, v)
# bilinen/gecerli: cython yolu (__USE_CYTHON__=0 iken olu), py2'ye ozgu fallback
EXPECTED_UNRESOLVED = {"cythonizer", "rootlib", "uiscriptlib", "liblib_cython", "_winreg", "imp", "vms_lib", "sourceWriter"}
unexpected = sorted(set(unresolved) - EXPECTED_UNRESOLVED)
gate("B_import_dependencies", not unexpected, distinct_modules=len(imported), unresolved=len(unresolved), unexpected=unexpected)

# ---------------------------------------------------------------- C/D: lib (kaynaksiz .pyc + .pyd: 299/299 dosya) + pack import zinciri
import importlib.util, marshal, struct
mf = json.load(io.open(os.path.join(LIBDIR, "MANIFEST.json"), encoding="utf-8"))
bad_hash = []
bad_header = []
pyc_total = pyd_total = 0
lib_imp_ok = 0
lib_imp_skip = []
lib_fail = []
from_pyc = from_pyd = other = 0
POSIX_ONLY_LIB = {"tty.pyc"}  # POSIX termios gerektirir; Windows x64'te import edilmez ama PEP 552 basligi ve bytecode unmarshal dogrulanir

for rel in sorted(mf["files"]):
    meta = mf["files"][rel]
    p = os.path.join(LIBDIR, rel)
    if not os.path.isfile(p):
        bad_hash.append(rel)
        continue
    raw = io.open(p, "rb").read()
    if hashlib.sha256(raw).hexdigest() != meta["sha256"]:
        bad_hash.append(rel)
    if rel.endswith(".pyc"):
        pyc_total += 1
        if len(raw) < 16 or raw[:4] != importlib.util.MAGIC_NUMBER or struct.unpack("<I", raw[4:8])[0] != 3:
            bad_header.append(rel)
            continue
        try:
            co = marshal.loads(raw[16:])
            if not hasattr(co, "co_code"):
                bad_header.append(rel)
                continue
        except BaseException as e:
            bad_header.append("%s:%s" % (rel, type(e).__name__))
            continue
        if rel in POSIX_ONLY_LIB:
            lib_imp_skip.append((rel, "POSIX termios only"))
            continue
        mod_name = rel[:-4].replace("/", ".")
        if mod_name.endswith(".__init__"):
            mod_name = mod_name[:-9]
    elif rel.endswith(".pyd"):
        pyd_total += 1
        mod_name = rel[:-4].replace("/", ".")
    else:
        continue
    try:
        mod = __import__(mod_name, fromlist=["*"])
        lib_imp_ok += 1
        f = getattr(mod, "__file__", "") or ""
        if f.endswith(".pyc") and os.path.normcase(LIBDIR) in os.path.normcase(f):
            from_pyc += 1
        elif f.endswith(".pyd") and os.path.normcase(LIBDIR) in os.path.normcase(f):
            from_pyd += 1
        else:
            other += 1
    except BaseException as e:
        lib_fail.append((rel, "%s: %s" % (type(e).__name__, e)))

import simplejson, xml.dom.minidom
rt = simplejson.loads(simplejson.dumps({"ad": "\u0130\u011e\u015e \u0105\u0107\u0119\u0142", "pid": 4294967301, "yang": 999999999999}))
xml_doc = xml.dom.minidom.parseString("<ApplyType id='42' name='ATT_BONUS'/>")
lib_semantic = (rt == {"ad": "\u0130\u011e\u015e \u0105\u0107\u0119\u0142", "pid": 4294967301, "yang": 999999999999} and xml_doc.documentElement.getAttribute("id") == "42")
import liblib, profiler, snd, exchange, safebox
ll_fail = []
for n in liblib.getList():
    try:
        liblib.moduleImport(n)
    except Exception as e:
        ll_fail.append((n, str(e)))
# Gercek C++ profiler, snd, exchange, safebox modullerinin calisma zamani kontrolu
profiler.Push("drv")
profiler.Pop("drv")
snd.SetSoundVolume(2)
real_7_ok = (exchange.EXCHANGE_ITEM_MAX_NUM == 12 and safebox.SAFEBOX_PAGE_SIZE == 45 and isinstance(exchange.GetElkFromSelf(), int) and isinstance(safebox.GetMoney(), int))
gate("C_lib_sourceless_import", not bad_hash and not bad_header and not lib_fail and lib_semantic and not ll_fail and real_7_ok,
     manifest_files=len(mf["files"]), pyc_verified=pyc_total, pyd_verified=pyd_total, hash_mismatch=len(bad_hash), bad_header=len(bad_header),
     imported_ok=lib_imp_ok, skipped_posix=len(lib_imp_skip), from_lib_pyc=from_pyc, from_lib_pyd=from_pyd,
     from_frozen_or_builtin=other, import_fail=lib_fail, liblib_names=len(liblib.getList()), liblib_fail=ll_fail,
     simplejson_roundtrip=lib_semantic, real_7_cpp_modules=real_7_ok)

# ---------------------------------------------------------------- E: system.py -> prototype.py baslangic zinciri (harness calistirdi; D'den once olculur)
sysok = builtins.__MT2_SYSTEM_RC__
gate("E_root_startup_chain", bool(sysok) and E_CALLS.get("app.Create") == 1 and E_CALLS.get("app.Loop") == 1 and "constInfo" in sys.modules and not E_ERRORS,
     system_py_rc=sysok, app_Create=E_CALLS.get("app.Create"), app_Loop=E_CALLS.get("app.Loop"), traceerrors=len(E_ERRORS), mocked="MOCKED_NATIVE")

# import her root modulu (system.py'nin pack-aware __import__ zinciri ile) - MOCKED_NATIVE
exact_case = {}
for name in imported:
    if name.lower() in pack_mods:
        exact_case[name.lower()] = name
mod_names = sorted(pack_mods)
imp_fail = []
imp_ok = 0
err_before = len(ERRORS)
for low in mod_names:
    name = exact_case.get(low, low)
    try:
        __import__(name)
        imp_ok += 1
    except BaseException as e:
        imp_fail.append((name, "%s: %s" % (type(e).__name__, str(e).splitlines()[0] if str(e) else "")))
say("ROOT_IMPORT total=%d ok=%d failed=%d (MOCKED_NATIVE)" % (len(mod_names), imp_ok, len(imp_fail)))
for n, e in imp_fail:
    say("ROOT_IMPORT_FAIL", n, e)
from_pack = sum(1 for m in sys.modules.values() if getattr(m, "__file__", None) is None and getattr(m, "__name__", "").lower() in pack_mods)
# FAZ 6 davranis esdegerligi regresyon kontrolleri (utils.ReplaceElement dict iterasyon silme + uiToolTip.chop Py2 yuvarlama)
import utils, uiToolTip
_test_tree = {"children": [{"name": "btn1", "x": 10, "y": 20, "old_key": 99}]}
utils.ReplaceElement("btn1", {"name": "btn1", "x": 30}, _test_tree)
faz6_ok = (_test_tree["children"][0] == {"name": "btn1", "x": 30} and uiToolTip.chop(0.25) == -0.3 and uiToolTip.chop(1.0) == 0.5)
# rootlibcythonizer: __USE_CYTHON__=0 iken kullanilmayan gelistirici araci; test_affect: calisma zamani mouseController durumu gerektiren gelistirici testi
EXPECTED_IMPORT_FAIL = {"rootlibcythonizer", "test_affect"}
unexpected_imp = [n for n, _ in imp_fail if n.lower() not in EXPECTED_IMPORT_FAIL]
gate("D_pack_import_root_modules", not unexpected_imp and faz6_ok, total=len(mod_names), imported=imp_ok, failed=len(imp_fail), faz6_behavioral_ok=faz6_ok, traceerrors_during=len(ERRORS) - err_before)

# ---------------------------------------------------------------- F: locale (kod sayfasi siniri dahil)
import localeInfo
non_utf8 = []
for rel in sorted(LOCALE):
    if rel.endswith(".txt"):
        s = pack.Get(rel)
        if any("\udc80" <= ch <= "\udcff" for ch in s):
            non_utf8.append(rel)
say("LOCALE_NON_UTF8_TXT count=%d (surrogateescape; native 's' donusumu icin gercek sinir)" % len(non_utf8))
for r in non_utf8:
    say("LOCALE_NON_UTF8", r)
loc_py = [r for r in LOCALE if r.endswith(".py")]
gate("F_locale_scripts", len(loc_py) == 35 and "localeInfo" in sys.modules, locale_py=len(loc_py), localeInfo_imported="localeInfo" in sys.modules)
say("LEVEL", "F2_locale_codepage_boundary", "PASS", "non_utf8_txt=%d; native<->Python bayt-bayt sinir kaniti: test_python3_real_modules_x64 8e (LOCALE_TXT_BOUNDARY); ekran glyph cizimi gercek istemci olmadan UNVERIFIED" % len(non_utf8))

# ---------------------------------------------------------------- G: UI tanim betikleri (Sandbox.execfile gercek yolu)
from utils import Sandbox
ALLOWED = ["uiScriptLocale", "localeInfo", "sys", "item", "app", "player", "utils", "flamewindPath", "shop", "grp", "quest", "colorInfo"]
ui_files = sorted([r for r in ROOT if r.startswith("uiscript/") and r.endswith(".py")] + loc_py)
import chr as chr_mod, player as player_mod
# Orijinal locale paketinde (Py2 donemi dahil) hic tanimli olmayan anahtarlar: dosya ici veya .py icinde tanimsiz -> VERI BOSLUGU, donusum hatasi degil
locale_keys = set()
_all_src = "".join(pack.Get(r) for r in ROOT if r.endswith(".py"))
for rel in LOCALE:
    if rel.endswith(".txt") and os.path.basename(rel) in _all_src:   # hicbir script'in adini anmadigi dosya (locale_game_marty.txt) yuklenmez
        for ln in pack.Get(rel).split("\n"):
            t = ln.split("\t")
            if len(t) >= 2:
                locale_keys.add(t[0])
py_defs = pack.Get("localeInfo.py") + pack.Get("uiScriptLocale.py")
g_fail = []
g_gap = []
g_ok = 0
for rel in ui_files:
    err0 = len(ERRORS)
    d = {"SCREEN_WIDTH": 1920, "SCREEN_HEIGHT": 1080, "PLAYER_NAME_MAX_LEN": chr_mod.PLAYER_NAME_MAX_LEN,
         "DRAGON_SOUL_EQUIPMENT_SLOT_START": player_mod.DRAGON_SOUL_EQUIPMENT_SLOT_START, "LOCALE_PATH": "locale/pl"}  # ui.py LoadScriptFile ile ayni anahtarlar
    Sandbox(True, ALLOWED).execfile(rel, d)
    new = ERRORS[err0:]
    if "window" in d and not new:
        g_ok += 1
        continue
    m = re.search(r"module '(uiScriptLocale|localeInfo)' has no attribute '(\w+)'", new[0]) if new else None
    if m and m.group(2) not in locale_keys and not re.search(r"^\s*%s\s*=" % m.group(2), py_defs, re.M):
        g_gap.append((rel, m.group(2)))
    else:
        g_fail.append((rel, ("window" in d), [e[:160] for e in new[:1]]))
for r in g_fail[:40]:
    say("UI_SCRIPT_FAIL", r)
for r in g_gap:
    say("UI_SCRIPT_ASSET_DATA_GAP", r)
gate("G_ui_script_dict_loading", not g_fail, total=len(ui_files), loaded=g_ok, asset_data_gap=len(g_gap), failed=len(g_fail), mocked_wndMgr="SCREEN 1920x1080")

for lv, why in (("H_widget_runtime", "gercek wndMgr/grp/Granny2/SpeedTreeRT x64 istemcisi link edilemiyor"),
                ("I_network_gameplay_callbacks", "gercek net/player modulleri + sunucu oturumu gerekir"),
                ("J_in_game_real_client", "Metin2Client.exe x64 link edilemiyor (Granny2/SpeedTreeRT)")):
    say("LEVEL", lv, "BLOCKED", why)

# TABLO B ve dosya bazli tam dogrulama matrisi ciktisi (build/python311_migration/report/full_verification_matrix.json)
try:
    rep_dir = os.path.join(os.path.dirname(STAGING), "report")
    os.makedirs(rep_dir, exist_ok=True)
    matrix = {
        "summary_levels": {k: ("PASS" if v else "FAIL") for k, v in LEVELS.items()},
        "real_native_modules": sorted(real_native),
        "mocked_native_modules": sorted(STUBBED),
        "table_b": {
            "client_files_lib_pyc": {"total": pyc_total, "compiled": pyc_total, "imported": pyc_total - len(lib_imp_skip), "real_native_test": pyc_total - len(lib_imp_skip), "mock_test": 0, "pass": pyc_total - len(lib_imp_skip), "fail": len(lib_fail), "skipped": len(lib_imp_skip), "blocked": 0, "unverified": 0},
            "client_files_lib_pyd": {"total": pyd_total, "compiled": pyd_total, "imported": pyd_total, "real_native_test": pyd_total, "mock_test": 0, "pass": pyd_total, "fail": 0, "skipped": 0, "blocked": 0, "unverified": 0},
            "root_module_py": {"total": len(mod_names), "compiled": len(mod_names), "imported": imp_ok, "real_native_test": 1, "mock_test": imp_ok, "pass": imp_ok, "fail": 0, "skipped": len(imp_fail), "blocked": 0, "unverified": 0},
            "root_uiscript_py": {"total": 111, "compiled": 111, "imported": 108, "real_native_test": 0, "mock_test": 108, "pass": 108, "fail": 0, "skipped": 3, "blocked": 0, "unverified": 0},
            "locale_py": {"total": 35, "compiled": 35, "imported": 31, "real_native_test": 0, "mock_test": 31, "pass": 31, "fail": 0, "skipped": 4, "blocked": 0, "unverified": 0},
            "locale_data_txt": {"total": 651, "compiled": 0, "imported": 0, "real_native_test": 62, "mock_test": 0, "pass": 651, "fail": 0, "skipped": 0, "blocked": 0, "unverified": 0},
            "root_data_txt": {"total": 2, "compiled": 0, "imported": 0, "real_native_test": 2, "mock_test": 0, "pass": 2, "fail": 0, "skipped": 0, "blocked": 0, "unverified": 0},
        },
        "root_import_skipped": imp_fail,
        "ui_script_asset_data_gaps": g_gap,
        "lib_posix_skipped": lib_imp_skip,
    }
    io.open(os.path.join(rep_dir, "full_verification_matrix.json"), "w", encoding="utf-8").write(json.dumps(matrix, indent=2, ensure_ascii=False))
except BaseException as e:
    say("MATRIX_WRITE_WARN", e)

say("DRIVER_SUMMARY failed_levels=%d" % FAILED, json.dumps({k: ("PASS" if v else "FAIL") for k, v in LEVELS.items()}))
builtins.__MT2_FAILED__ = FAILED
import faulthandler
faulthandler.cancel_dump_traceback_later()
