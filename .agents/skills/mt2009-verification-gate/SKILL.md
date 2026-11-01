---
name: mt2009-verification-gate
description: "Enforces truthful MT2009 Git/CI build and test reporting with source, object, linker, runtime and live gameplay evidence distinguished. Use before declaring a fix complete, merging a branch, releasing client/server changes or reporting PASS."
---
# MT2009 verification and release gate
## Procedure
1. Record branch, exact HEAD, origin tracking SHA, clean/dirty state, changed files and scope before review; confirm all expected changes are present in the reviewed diff.
2. Construct a matrix: static inspection, MSVC x64 /Zs, MSVC x64 object compilation, Win x64 linking/PE32+, Python 2.7 actual runtime, GCC Windows test suite, isolated server code-path tests, FreeBSD amd64 build, client gameplay smoke and optional sanitizers.
3. Run only applicable checks with available tools and confirmed <=50% CPU hard cap, never assume prior logs correspond to this HEAD. Record exact command, exit code and the test actually exercised.
4. Compare assertions to current code; a server-only test does not prove cross-header ABI. A numerical model does not prove Python C API runtime. -fsyntax-only does not prove ASan/UBSan execution. A successful compile does not prove link or gameplay.
5. Before any merge, review lifetime/error paths, rollback strategy, dependencies and outstanding security defects. Preserve isolated branches when the user has not explicitly approved merge. Do not auto-push or deploy.
6. Summarize what is VERIFIED (by concrete test), what is SOURCE_REVIEWED, what is BLOCKED, NOT_RUN, REGRESSED or UNKNOWN, and the next smallest unblocker.
## Python migration evidence gate
- Python 2.7 x86 C API PASS does not prove CPython 3 x64 compatibility. For any Python 3 port, require separately: selected x64 Python DLL/import-lib architecture; actual C++20 x64 link/execute bootstrap; genuine Python 3 module import/C API test; archived game script import; full x64 game link; and isolated gameplay smoke. Mark each independently, using `/mt2009-python3-cpp20-migration`.

## Acceptance gate
- Every PASS cites a command/test/environment/SHA; no inferred or fabricated evidence.
- Unmet critical gates prevent claims of a native x64 playable release.

## Project safety contract
- Scope: D:\mt22009; Windows native x64 client with a FreeBSD amd64 server. Inspect current branch, HEAD and worktree before changes. Never assume a historical report reflects current code.
- Default to read-only investigation and a narrow plan. Make code edits, branch changes, commits and pushes only if the current task explicitly authorizes each action. Never merge, deploy, restart production, change DB/schema, or alter a running FreeBSD server without explicit separate authorization.
- Granny source upgrade is deferred. Do not modify Granny, other third-party SDKs, external binaries or vendor licenses as a side effect.
- Keep all build/test child processes within the project's <=50% total CPU hard cap. Verify the existing tests/run_limited.ps1 syntax and actual enforcement first; if you cannot guarantee the cap, stop instead of running unrestricted builds/tests.
- Preserve wire compatibility and intended server behavior. Do not perform broad data-type replacements, speculative version changes or silent protocol edits.
- Distinguish factual code inspection, /Zs syntax+static_assert validation, object compilation, successful x64 linking, and live gameplay tests. Never use one as evidence for another.
- Report evidence with file:line, exact commands and exit codes, reproducible cases, impact, remaining BLOCKED/UNVERIFIED items. No fake PASS; changes are not complete until relevant tests really ran.
