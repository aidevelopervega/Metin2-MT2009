# MT2009 - python311_native_stubs.py
# *** MOCKED_NATIVE ***  Gercek istemci C++ modulleri (app, wndMgr, player, net, grp, ...) bu test ortaminda
# LINK EDILEMEZ (Granny2/SpeedTreeRT x64 SDK yok). Bu dosya yalnizca o modullerin ADLARINI taklit eder; sonuclari
# "gercek oyun entegrasyonu" olarak SUNULAMAZ. Gercek olanlar: pack, dbg, liblib (C++ uretim kaynaklari).
import sys, types, zlib, importlib.abc, importlib.machinery

STUBBED = (
    "acce", "app", "appPlayerSettingsModule", "background", "captcha", "chat", "chr", "chrmgr", "effect", "event",
    "eventMgr", "exchange", "fly", "grp", "grpImage", "grpText", "grpThing", "guild", "ikashop", "ime", "item",
    "messenger", "miniMap", "net", "nonplayer", "player", "profiler", "quest", "safebox", "ServerStateChecker",
    "shop", "skill", "snd", "systemSetting", "textTail", "udp", "wndMgr", "wndSlot", "wndText",
)
CALLS = {}      # "modul.fonksiyon" -> cagri sayisi (kanit)
OVERRIDES = {
    "wndMgr.GetScreenWidth": 1920, "wndMgr.GetScreenHeight": 1080,
    "systemSetting.GetWidth": 1920, "systemSetting.GetHeight": 1080,
    "systemSetting.GetScaledResolution": (1920, 1080), "systemSetting.IsScaleMode": 0,
    "app.GetLocalePath": "locale/pl", "app.GetLocaleServiceName": "EUROPE", "app.GetDefaultCodePage": 1250, "systemSetting.GetLanguage": "pl",
    "systemSetting.GetFontName": "Tahoma", "systemSetting.GetFontSize": 12,
}


import glob, re, os
# app.ENABLE_* : gercek istemcide #ifdef ENABLE_X -> 1 / 0 sabitleri. Burada ayni karar, istemci basliklarindaki AKTIF #define satirlarindan turetilir.
_ACTIVE_DEFINES = set()
for _h in glob.glob(os.path.join("client", "src", "**", "*.h"), recursive=True):
    try:
        for _line in open(_h, encoding="utf-8", errors="replace"):
            _m = re.match(r"\s*#\s*define\s+([A-Za-z_0-9]*ENABLE[A-Za-z_0-9]*)\b", _line)
            if _m:
                _ACTIVE_DEFINES.add(_m.group(1))
    except OSError:
        pass


def _is_const(name):
    return name[:1].isupper() and name.upper() == name


_PROTOCOL_ATTRS = {"__path__", "__file__", "__wrapped__", "__all__", "__spec__", "__loader__", "__package__", "__cached__",
                   "__dict__", "__class__", "__mro_entries__", "__origin__", "__getstate__", "__bases__"}


class _Stub(types.ModuleType):
    def __getattr__(self, name):
        if name in _PROTOCOL_ATTRS:
            raise AttributeError(name)
        full = self.__name__ + "." + name
        if _is_const(name):
            value = (1 if name in _ACTIVE_DEFINES else 0) if "ENABLE" in name else 1 + (zlib.crc32(full.encode()) & 0x7FFF)
        elif full in OVERRIDES:
            ov = OVERRIDES[full]
            value = (lambda *a, **k: _count(full, ov))
        else:
            value = (lambda *a, **k: _count(full, 0))
        setattr(self, name, value)
        return value


def _count(full, ret):
    CALLS[full] = CALLS.get(full, 0) + 1
    return ret


class _Finder(importlib.abc.MetaPathFinder, importlib.abc.Loader):
    def find_spec(self, name, path=None, target=None):
        if name in STUBBED:
            return importlib.machinery.ModuleSpec(name, self)
        return None

    def create_module(self, spec):
        return _Stub(spec.name)

    def exec_module(self, module):
        pass


if not any(isinstance(f, _Finder) for f in sys.meta_path):
    sys.meta_path.append(_Finder())

# Gercek C++ dbg modulu uzerinde yakalama: TraceError kaydedilir (gercek cagri da yapilir); LogBox (MessageBox) engellenir.
import dbg
ERRORS = []
LOGBOXES = []
_orig_trace_error = dbg.TraceError


def _capture_trace_error(*a):
    ERRORS.append(" ".join(str(x) for x in a))
    return _orig_trace_error(*a)


dbg.TraceError = _capture_trace_error
dbg.LogBox = lambda *a: LOGBOXES.append(" ".join(str(x) for x in a))
import faulthandler
faulthandler.dump_traceback_later(120, exit=True, file=sys.__stderr__)   # askida kalma korumasi (driver sonunda iptal edilir)
print("STUBS installed:", len(STUBBED), "MOCKED_NATIVE modules", file=sys.__stdout__)
