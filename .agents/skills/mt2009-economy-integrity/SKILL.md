---
name: mt2009-economy-integrity
description: "Audits Metin2 MT2009 Yang and item transaction integrity in shop, inventory, safebox, exchange/trade, refine and offline-shop paths. Use for duplication, negative-price, integer-overflow and replay/race-condition checks without changing production economy."
---
# MT2009 economy and item integrity
## Procedure
1. Follow server-side state changes for shop buy/sell, safebox deposit/withdrawal, item transfer, refine, trade and offline shop, one subsystem at a time. Map packet entry, validation, authority, item/coin mutations, persistence, rollback and logging.
2. Check signed YANG/int64 vs ITEM_COUNT/uint32 vs item identity/uint64. Test arithmetic overflow, negative amount, over-limit cost, invalid slot/window, stale item handles and repeated/duplicated request behavior.
3. Verify that atomicity assumptions are actually implemented; where async DB or multi-stage operations exist, identify partial commit, disconnect and retry edge cases. Do not declare SQL transactions exist unless sourced in code.
4. Use controlled offline fixtures to test double-spend, duplication, packet replay and concurrent requests; do not execute hostile traffic on production.
5. Preserve current game balance limits (e.g. repository GOLD_MAX) and existing gameplay semantics. Security fixes must not covertly increase item or currency supply.
## Acceptance gate
- No proven duplicate or lost asset under tested failure/retry cases; unknown concurrency paths stay UNVERIFIED.
- Test evidence ties back to real production handlers and persistence code, not just value-type tests.

## Project safety contract
- Scope: D:\mt22009; Windows native x64 client with a FreeBSD amd64 server. Inspect current branch, HEAD and worktree before changes. Never assume a historical report reflects current code.
- Default to read-only investigation and a narrow plan. Make code edits, branch changes, commits and pushes only if the current task explicitly authorizes each action. Never merge, deploy, restart production, change DB/schema, or alter a running FreeBSD server without explicit separate authorization.
- Granny source upgrade is deferred. Do not modify Granny, other third-party SDKs, external binaries or vendor licenses as a side effect.
- Keep all build/test child processes within the project's <=50% total CPU hard cap. Verify the existing tests/run_limited.ps1 syntax and actual enforcement first; if you cannot guarantee the cap, stop instead of running unrestricted builds/tests.
- Preserve wire compatibility and intended server behavior. Do not perform broad data-type replacements, speculative version changes or silent protocol edits.
- Distinguish factual code inspection, /Zs syntax+static_assert validation, object compilation, successful x64 linking, and live gameplay tests. Never use one as evidence for another.
- Report evidence with file:line, exact commands and exit codes, reproducible cases, impact, remaining BLOCKED/UNVERIFIED items. No fake PASS; changes are not complete until relevant tests really ran.
