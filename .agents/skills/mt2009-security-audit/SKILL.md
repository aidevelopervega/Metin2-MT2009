---
name: mt2009-security-audit
description: "Reviews MT2009 Metin2 C++ server and client for memory safety, packet validation, double-free, use-after-free, authorization and exploit risks. Use for security audits of ShopEx, game server, network parsers, login, inventory and trade."
---
# MT2009 security audit
## Purpose
Prove or reject concrete vulnerabilities in production code without endangering the live server.
## Procedure
1. Identify trust boundaries: client packet fields, Python C API, login/session, quest inputs, SQL requests, admin permissions, external assets and vendor libraries.
2. Trace input to side effects, enumerating caller/callee and active feature macros. Check wire length, count x size overflow, signedness, OOB access, dangling references, TOCTOU and failure cleanup.
3. For C++ lifetime inspect all owning and non-owning maps/vectors, destructor calls, reloads, repeated Destroy, shared pointers and error paths. For ShopEx inspect AddGuest -> StartShopping, AddShopTable, ReadShopTableEx, ConvertToShopItemTable.
4. Reproduce safely with minimal offline fixtures or existing --check-lua / unit harness. Use ASan/UBSan only where instrumented runtime is available; compilation with -fsanitize and -fsyntax-only is not a sanitizer runtime result. Never trigger corruption on production or exposed endpoints.
5. Fix only issues supported by evidence when authorized. Require a regression that invokes the real affected code path, not just a mathematical model.
6. Report severity, reachable trigger, impact, test evidence and mitigation. An untested theoretical condition remains a risk, not a confirmed live exploit.
## Embedded Python migration security
- When replacing Python 2.7, coordinate with `/mt2009-python3-cpp20-migration` for CPython 3 x64 binary provenance, GIL/refcount/module lifetime, rejected unsigned wraps, Unicode/bytes boundary checks, import hooks, DLL search paths and untrusted script exposure. New Python alone is not a sandbox or a guarantee against exploitation.

## Acceptance gate
- No out-of-bounds write/read in the targeted path under tested boundary cases.
- Failure leaves session, packet queue, money/item state and ownership maps consistent.
- Any remaining cross-platform or sanitizer gaps are marked BLOCKED/UNVERIFIED.

## Project safety contract
- Scope: D:\mt22009; Windows native x64 client with a FreeBSD amd64 server. Inspect current branch, HEAD and worktree before changes. Never assume a historical report reflects current code.
- Default to read-only investigation and a narrow plan. Make code edits, branch changes, commits and pushes only if the current task explicitly authorizes each action. Never merge, deploy, restart production, change DB/schema, or alter a running FreeBSD server without explicit separate authorization.
- Granny source upgrade is deferred. Do not modify Granny, other third-party SDKs, external binaries or vendor licenses as a side effect.
- Keep all build/test child processes within the project's <=50% total CPU hard cap. Verify the existing tests/run_limited.ps1 syntax and actual enforcement first; if you cannot guarantee the cap, stop instead of running unrestricted builds/tests.
- Preserve wire compatibility and intended server behavior. Do not perform broad data-type replacements, speculative version changes or silent protocol edits.
- Distinguish factual code inspection, /Zs syntax+static_assert validation, object compilation, successful x64 linking, and live gameplay tests. Never use one as evidence for another.
- Report evidence with file:line, exact commands and exit codes, reproducible cases, impact, remaining BLOCKED/UNVERIFIED items. No fake PASS; changes are not complete until relevant tests really ran.
