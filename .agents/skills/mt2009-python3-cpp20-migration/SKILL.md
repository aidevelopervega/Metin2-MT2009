---
name: mt2009-python3-cpp20-migration
description: "Plans and safely ports the MT2009 Windows x64 C++20 embedded Python 2.7 client to modern CPython 3.x: interpreter selection, x64 Python ABI, PyInt/PyLong and PyString/PyUnicode C API, PyModuleDef/PyInit, UI scripts, packaging, GIL, 64-bit IDs/Yang, testing and rollback. Use when investigating or implementing Python3/C++20 compatibility; do not blindly upgrade DLLs."
---
# MT2009 — Embedded Python 2.7 -> Python 3.x / C++20 migration

## Mission and limits
Modernize the **embedded client interpreter and bindings** while preserving real MT2009 gameplay and the FreeBSD server contract. C++20 and Python 3 are **different compatibility axes**: `/std:c++20` does not upgrade Python, and changing `python27.lib` to `python3*.lib` is not a migration. The Windows PE32+ executable needs an **AMD64** CPython runtime and matching compiler/runtime/library artifacts, not an x86 DLL relabeled x64. Python 3 upgrade may remove Python 2.7 EOL constraints but does **not** resolve Granny/Miles/SpeedTree/D3D8 blockers or automatically speed up FPS.

## Mandatory operating mode
Stage 0 is always read-only unless the current user task explicitly approves implementation. Begin with repository truth and prior branches: check SHA/dirty worktree; do not mix Python port with current `fix/shopex-bounds-hardening` or other outstanding work. Propose the smallest independently testable vertical slice. Preserve behavior and avoid silent mass replacements.

## Stage 0 — Inventory, dependency mapping and target decision (READ-ONLY)
1. Locate the *actual* embedded interpreter initialization/finalization, `python27.lib`/DLL and Python27 headers, VC projects/CMake flags and x86-only linkage. Search `client/src/ScriptLib`, `UserInterface`, `EterPythonLib`, `EterPack`, `extern/client`, launch/packing scripts and real Python asset archives. Do not infer installed DLLs prove their load path.
2. Inventory all C API exports and module entry points (`Py_InitModule`, `init*`, `PyInt_*`, `PyLong_*`, `PyString_*`, `PyUnicode_*`, `PyFile_*`, `PyCObject_*`, `Py_BuildValue`, `PyArg_ParseTuple`, `PyTuple_GetItem`, `PyErr_*`, `PyEval_*`, `Py_Initialize`, import hooks). Identify cross-module reference-ownership and 64-bit parsing hazards; annotate file:line.
3. Inventory **all executed Python 2 source**, packed scripts, stdlib assumptions, bytecode (`.pyc` version incompatible), third-party C extensions and scripts loaded dynamically; distinguish plain `.py`, archived/encrypted assets and genuine binary `.pyd` dependencies. Do not try to decrypt/unpack protected material without authorization.
4. Propose **candidate GIL-enabled, stable AMD64 CPython 3.x targets** (e.g. 3.13 and 3.14, subject to tested dependencies, support/security status and build chain). Compare per-version availability, vendor support, API removals, Windows MSVC toolset, embedding/linkage and extension compatibility. Recommend **one version only after measured dependency compatibility**; avoid automatically selecting the highest release.
5. Build a dependency/effort matrix and rollback plan. Identify an independently testable minimal module (`player`, `net`, `guild`, or basic bootstrap) and map all blockers to owners. Do not assert full feasibility without a script inventory.

## Stage 1 — Isolated interpreter + C++20 bootstrap (only with authorization)
1. Use a separately approved local branch and reproducible x64 Python installation/redistribution method. Never overwrite the existing Python 2.7 interpreter or its production assets. Preserve the functioning baseline. No automatic public push.
2. Compile one isolated C++20 x64 embed test with target `Python.h`, AMD64 Python import library and DLL. Log actual compiler, architecture and Python `sys.version`/`sys.maxsize`; validate `Py_Initialize`/`Py_FinalizeEx`, interpreter search path, stdlib loading, failure/cleanup and Windows DLL search safety. **A standalone bootstrap is NOT a running Metin2 client.**
3. Where appropriate use `PyConfig`/`Py_InitializeFromConfig` for modern initialization and explicit UTF-8/wide-character path configuration; verify APIs against the selected exact Python version. Do not copy legacy `Py_SetPythonHome(char*)` patterns blindly or rely on global `PATH` for unsafe DLL loading.
4. Keep CPython regular GIL builds first. CPython 3.13+ free-threaded variants are a separate project requiring C-extension thread-safety audit, no borrowed-reference races and compatible binary modules; do not activate to chase parallel CPU performance.

## Stage 2 — Real C API / module conversion
1. Replace Python 2 `init<module>()` / `Py_InitModule*` only after mapping module registration; adopt Python 3 `PyModuleDef`, `PyInit_<module>()` and embed-time `PyImport_AppendInittab` if modules are built in. Correct exception returns, `PyObject*` ownership/`Py_DECREF`, and method calling conventions.
2. Map `PyInt` to Python 3 `PyLong` where semantics permit. Use `PyLong_AsLongLong`, `PyLong_AsUnsignedLongLong` (NOT Mask) + explicit `PyErr_Occurred`; preserve Python exception contracts and reject invalid/negative/overflow IDs. `uint64_t` (`PID`, `AID`, `GUILD_ID`) must survive `>2^32` and `>2^53`; `YANG`/points are signed `int64_t` with server limits. Never conflate integer semantics and wire representation.
3. Treat `PyString` -> `PyUnicode` vs `PyBytes` as an **interface-by-interface decision**. UI labels, player names and text use a documented encoding/Unicode policy; network binary frames, packed assets, compressed buffers and file blobs remain bytes. Test embedded NUL, non-ASCII Turkish text and malformed UTF-8/legacy encodings.
4. Review Python 3 changed format codes (`Py_BuildValue`, `PyArg_ParseTuple`), `PyUnicode_AsUTF8` pointer lifetimes, borrowed/new refs, macros/type definitions, `PyObject` ownership and API availability. No global regex rename or `typedef PyInt PyLong` shim hiding incompatible behaviors.
5. Audit thread entry/GIL for game/render/network callbacks and teardown. Document lock boundaries without calling Python on an unsafe thread. Keep game loop performance and error propagation observable.

## Stage 3 — Game scripts, packaging and user-facing behavior
1. Port script syntax/semantics deliberately: `print`, exception syntax, `xrange`/`range`, `iteritems`/dict views, integer division (`/` vs `//`), `map`/`filter` iterators, comparisons/sorting, imports/package layout and removed stdlib APIs. Maintain feature parity for login, character select, game phase, item, shop, guild, chat, quest UI.
2. Rebuild Python 3 bytecode/assets with the **selected target interpreter**; Python 2 `.pyc` and binary `.pyd` artifacts are not reusable. Verify `EterPack`/virtual-file importer and `sys.path`/`sys.meta_path`, path casing and encoding. Never claim an import works if only the file exists.
3. Maintain 64-bit Python-C++ values, baseline UI event contracts and packet compatibility. Do not modify the FreeBSD protocol merely to make UI Python 3 code simpler.

## Stage 4 — Verification ladder (strict, incremental)
- G0: inventory, risk matrix, rollback and exact target chosen (source-reviewed, no runtime PASS).
- G1: C++20 x64 embed bootstrap **compiles, links AND executes** with version/machine evidence.
- G2: core modules genuinely import, exported functions bind, GIL/refcounts/exception paths pass C API runtime tests; x64-only linkage established.
- G3: real Python 3 game scripts import/run from their *actual* package/archive and Turkish/non-ASCII UI strings render correctly.
- G4: in an isolated game client, **real x64 executable** connects to isolated compatible server, login -> select -> map -> movement -> inventory -> shop -> logout/relogin; confirm crashes, logs and persistence.
- G5: measure startup time, frame-time p50/p95/p99, memory, CPU and load/network behavior against Python 2 baseline where available; reject regressions and record a rollback mechanism. Do not claim speed improvement merely because CPython is newer.
- Run security checks for package imports, DLL search path, input validation, extension ABI, malformed network data, Unicode and reference-count leaks. Interpret sanitizer/test-model evidence correctly: `/Zs` != x64 link; server-only packet assertions != live client; Python 2 runtime tests != Python 3 tests.

## Deliverables and stopping rules
Produce: inventory table; candidate/selected Python version rationale; C API and script porting matrix with file:line; dependencies by architecture/ABI; milestone plan; verified test matrix; explicit unresolved blockers; risk/rollback; only approved small code diffs. If no x64 Python dependencies exist or the binary can't link, mark **BLOCKED** rather than force fake success. Do not declare the game playable until G4.

## Python 3 migration matrix and acceptance sheet

| Area | Python 2.7 / legacy candidate | Python 3 / C++20 migration decision | Required evidence |
|---|---|---|---|
| Runtime | python27.lib, python27.dll, 32-bit COFF | Selected regular-GIL CPython 3.x **AMD64** lib + DLL, redistributable runtime | PE/COFF machine, runtime version, actual link and interpreter start |
| Initialization | Py_Initialize, legacy char* path, initmod | Version-correct PyConfig, Py_InitializeFromConfig, PyImport_AppendInittab | Linked isolated embed and import test |
| Extension module | Py_InitModule / initmodule | PyModuleDef / PyInit_module, correct return semantics | Import/export smoke using real CPython 3 |
| C integer | PyInt/PyLong, *_Mask, Py_BuildValue("i") | PyLong and checked signed/unsigned conversions, 64-bit preserved | Real C API test: -1,0,2^32+1,2^53+1,INT64 bounds,UINT64_MAX,overflow,invalid input |
| Strings | PyString, PyUnicode, implicit byte decoding | Explicit text=Unicode vs binary=bytes contract, encoding and lifetime audit | Turkish characters, embedded NUL, invalid bytes, file/packet roundtrip |
| Ownership | borrowed refs, error/exception return | Py_INCREF/Py_DECREF ownership table and exception-safe error exits | Runtime refcount/leak/error-propagation tests |
| Loop / GIL | Python code called during network/render callbacks | GIL ownership, game loop/thread affinity, teardown/re-init protocol | Instrumented game loop and failure tests |
| Scripts | Python 2 syntax, integer division, dict.iteritems, imports | Python 3 semantics and version-pinned source/bytecode assets | Game scripts import from actual archive and UI smoke |
| Third-party | Python2 .pyd modules, Boost.Python or other bindings | Version- and architecture-compatible replacements or recompile | Import/ABI test and license/provenance check |
| Game wire | PID/AID/GUILD_ID, YANG, TItemData | Preserve fixed-width C++ wire ABI and server contract | Client and server sizeof/offsetof plus actual packet handler test |
| Security | end-of-life runtime, ambiguous encodings, DLL hijack | Maintained runtime, bounded input, safe import/DLL path, correct exceptions | Audit + executable regression tests |
| Performance | Baseline Python2 UI/frame-time | Measured CPU/frame-time/memory against baseline; optimize only confirmed hotspots | Repeatable p50/p95/p99 and representative device results |

## Project safety contract
- Scope: `D:\mt22009`; Windows native x64 client, FreeBSD amd64 server. Read current `AGENTS.md`, `PROJECT_STATE.md`, actual Git HEAD/diff and applicable build files before action; old reports are not current truth.
- Default to a READ-ONLY audit and migration plan. No edits, builds, new branch, commit, merge, push, install or deployment without corresponding explicit authorization. Work on a separate, approved local branch/worktree if implementation is authorized, and preserve the stable client and existing feature branches.
- Never touch production FreeBSD server/DB, adjust protocol or schema, run paid FreeBSD CI, or change `main` as a side effect. Granny source/SDK upgrade is postponed; no other vendor SDK swaps without approval.
- All CPU-heavy compile/test child processes must obey the project `<=50%` hard cap; inspect `tests/run_limited.ps1` actual mode and enforcement before running. Avoid broad parallelism, resource-heavy conversion or unbounded recursive tooling.
- Preserve `uint64_t` IDs, `int64_t` Yang/points, fixed-width wire ABI, and server authority. Maintain source/test evidence for each migration change. No invented test results or fake x64 executable claims.
- Report current SHA, exact file/line and command/exit evidence; distinguish SOURCE_REVIEWED, BUILD_ONLY, VERIFIED, BLOCKED and NOT_RUN. Stop at agreed task scope.
