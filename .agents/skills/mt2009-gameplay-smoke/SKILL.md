---
name: mt2009-gameplay-smoke
description: "Defines and validates a real MT2009 playability vertical slice across login, character select, entering game, movement, combat, inventory, shop, persistence, disconnection and reconnection. Use for gameplay smoke/regression checks, distinguishing simulated tests from a working x64 client."
---
# MT2009 playable vertical slice
## Core playable path
Launch -> authenticate -> list/select character -> load map -> control movement/camera -> view actors -> attack/skills -> pickup item -> use/equip item -> open/buy/close NPC shop -> logout -> relogin -> verify persistence.
## Procedure
1. Check if a real PE32+ x64 executable exists, links and starts. If not, classify all UI/playability checks as BLOCKED and create a test checklist only; do not claim gameplay from packet/unit tests.
2. Identify isolated development server and safe test account/characters. No live economy manipulation, production account credentials or production load testing.
3. For each step record preconditions, exact user action, expected server-authoritative response, visual/audio result, client/server logs, latency and failure/retry behavior.
4. Test 64-bit IDs and Yang at boundaries with synthetic isolated fixtures where feasible. Cover death/respawn, disconnect/reconnect, zone transitions, inventory syncing and repeat shop interaction.
5. Verify usability separately from protocol: readable UI, keyboard/mouse controls, input latency, stuck states, broken animations, text/texture visibility, crashes and frame pacing.
## Python 3 UI compatibility
- If the client is ported to Python 3, invoke `/mt2009-python3-cpp20-migration` and add live UI smoke for login/select, locale/Turkish text, Unicode/bytes paths, import-from-pack, inventory/shop/guild/chat updates and teardown/relog. A Python 3 demo interpreter running is not playable game proof.

## Acceptance gate
- Screenshot/video/log evidence may support an actual run, but only real client execution counts as a runtime PASS.
- Each failed or unrun scenario is explicitly NOT_RUN/BLOCKED with a reproducible next step.

## Project safety contract
- Scope: D:\mt22009; Windows native x64 client with a FreeBSD amd64 server. Inspect current branch, HEAD and worktree before changes. Never assume a historical report reflects current code.
- Default to read-only investigation and a narrow plan. Make code edits, branch changes, commits and pushes only if the current task explicitly authorizes each action. Never merge, deploy, restart production, change DB/schema, or alter a running FreeBSD server without explicit separate authorization.
- Granny source upgrade is deferred. Do not modify Granny, other third-party SDKs, external binaries or vendor licenses as a side effect.
- Keep all build/test child processes within the project's <=50% total CPU hard cap. Verify the existing tests/run_limited.ps1 syntax and actual enforcement first; if you cannot guarantee the cap, stop instead of running unrestricted builds/tests.
- Preserve wire compatibility and intended server behavior. Do not perform broad data-type replacements, speculative version changes or silent protocol edits.
- Distinguish factual code inspection, /Zs syntax+static_assert validation, object compilation, successful x64 linking, and live gameplay tests. Never use one as evidence for another.
- Report evidence with file:line, exact commands and exit codes, reproducible cases, impact, remaining BLOCKED/UNVERIFIED items. No fake PASS; changes are not complete until relevant tests really ran.
