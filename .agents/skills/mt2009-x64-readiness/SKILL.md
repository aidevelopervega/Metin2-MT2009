---
name: mt2009-x64-readiness
description: "Finds and prioritizes MT2009 native Windows x64 client blockers, especially COFF i386 versus AMD64 libraries, Python27, Granny, Miles, SpeedTree, D3D8, inline assembly and linker/runtime readiness. Use for port planning without modifying third-party SDKs."
---
# MT2009 x64 executable readiness
## Goal
Shortest verifiable path to a real Windows PE32+ client launch, without disguising /Zs passes as a working game.
## Procedure
1. Read project build files, selected MSVC target architecture and current worktree. Inventory exact .lib/.dll/.obj dependencies used at link time; do not assume every extern file is linked.
2. Use existing installed tools such as dumpbin /headers, llvm-readobj or file, only if available, to classify each dependency: AMD64, i386, unknown, absent; document path and evidence. Check import-library vs static-library and its associated DLL architecture.
3. Build a blocker graph: mandatory at startup vs optional subsystem; API/ABI and C++ runtime compatibility; Python 2.7 interpreter/module architecture; inline x86 asm and D3D8/SDK runtime dependencies.
4. Produce a legal, maintainable option for each blocker: authorized x64 vendor SDK, supported equivalent interface, isolated adapter or separately approved renderer/engine port. No fabricated stubs, renaming x86 binaries, mixing architectures or fake version strings.
5. Prioritize the minimum vertical slice needed to create a real x64 .exe, load, authenticate to an isolated compatible server and render a character. Treat replacement of major rendering/animation libraries as separate decisions requiring approval.
6. Verify sequential gates: headers -> x64 object files -> actual linker -> PE32+ binary and dependent DLLs -> application launch -> real interaction.
## Python 3/C++20 alternate critical path
- Invoke `/mt2009-python3-cpp20-migration` for the **separate** Python interpreter/C API/script migration, rather than treating Python27-to-Python3 as a DLL rename. Compare AMD64, GIL-enabled CPython choices against current Python 2.7 dependencies and verify an isolated C++20 embed-runtime link first.
- A working Python 3 host does not prove the full x64 MT2009 client or eliminate Granny/Miles/SpeedTree/D3D8 blockers. Distinguish Python modernization from game-engine/x64 linkage.

## Acceptance gate
- Machine-readable or evidence-backed dependency matrix with owners and next decision.
- Clear minimal critical path; no claim of executable readiness while key libraries remain 32-bit or missing.

## Project safety contract
- Scope: D:\mt22009; Windows native x64 client with a FreeBSD amd64 server. Inspect current branch, HEAD and worktree before changes. Never assume a historical report reflects current code.
- Default to read-only investigation and a narrow plan. Make code edits, branch changes, commits and pushes only if the current task explicitly authorizes each action. Never merge, deploy, restart production, change DB/schema, or alter a running FreeBSD server without explicit separate authorization.
- Granny source upgrade is deferred. Do not modify Granny, other third-party SDKs, external binaries or vendor licenses as a side effect.
- Keep all build/test child processes within the project's <=50% total CPU hard cap. Verify the existing tests/run_limited.ps1 syntax and actual enforcement first; if you cannot guarantee the cap, stop instead of running unrestricted builds/tests.
- Preserve wire compatibility and intended server behavior. Do not perform broad data-type replacements, speculative version changes or silent protocol edits.
- Distinguish factual code inspection, /Zs syntax+static_assert validation, object compilation, successful x64 linking, and live gameplay tests. Never use one as evidence for another.
- Report evidence with file:line, exact commands and exit codes, reproducible cases, impact, remaining BLOCKED/UNVERIFIED items. No fake PASS; changes are not complete until relevant tests really ran.
