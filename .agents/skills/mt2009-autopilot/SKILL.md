---
name: mt2009-autopilot
description: "Resumable autonomous MT2009 native x64 C++20 engineering campaigns with the built-in /goal command, per-revision verification gates, nine specialist skills, and evidence-based task continuation. Use when coordinating long-running secure, playable x64 porting without manual report handoffs."
---
# MT2009 Autopilot — v2 (resume, revision-aware gates)

## Activation and scope
- This skill is an **instruction package, not a daemon**. Start or RESUME a genuine built-in Antigravity `/goal` session; confirm Active Goal in the UI. It cannot bypass model quota, context limits, permission prompts, platform crashes or closed sessions. Never imply it operates unattended when the platform does not.
- First read `D:\mt22009\AGENTS.md`, `PROJECT_STATE.md`, `docs/SERVER_X64_GOALS.md`, current Git status/branch/HEAD, and `scratch/autopilot/state.json`. **RESUME, do not restart/re-analyze completed tasks solely because the session is new.** `AGENTS.md` overrides this skill whenever rules conflict. Do not create root `GOAL.md` or new tracked Markdown outside `.agents/skills/**/SKILL.md`.
- All 8 specialist skills remain available: `mt2009-verification-gate`, `mt2009-protocol-abi`, `mt2009-security-audit`, `mt2009-economy-integrity`, `mt2009-x64-readiness`, `mt2009-python3-cpp20-migration`, `mt2009-performance-profile`, `mt2009-gameplay-smoke`. Load only the ones relevant to the current task. Read actual files; report real use.

## Non-negotiable campaign state
- **CAMPAIGN ACTIVE is distinct from a task's PASS.** Never mark the native x64 campaign complete merely because a milestone, ShopEx, protocol group or selected priority list passes. Work-stream statuses are separate, and **PASS is scoped to the exact code revision and platform**.
- The overall acceptance target remains a genuine **native Windows x64 client PE32+ compiled, linked and run with real x64 dependencies, and a verified playable vertical slice against the compatible FreeBSD amd64 server**. A C++20 `/Zs` pass proves only syntax/type-checking. `.obj` compilation, `link.exe /MACHINE:X64`, process boot and real login/movement/inventory/shop/logout are separate gates.
- A FreeBSD CI pass at HEAD=A does **not** validate server changes made at HEAD=B. Require an actual CI run with `headSha=B` (or a later descendant that includes B). In the checkpoint record exact `verified_head` and `run_id` for the FreeBSD gate. A client syntax PASS at B is likewise not native x64 runtime PASS.
- A dependency being `BLOCKED` (e.g. **Granny x64 SDK**) blocks only dependent steps; continue **independent safe** work such as Python3 x64 C-API migration in an isolated branch, x64 `.obj` compilation, remaining protocol runtime tests, and dependency inventory. **Granny source upgrade is DEFERRED**; do not modify it.
- Treat validation failures as failures, not as reasons to skip tests, rewrite production ABI, or fabricate passing evidence. Refuse irreversible actions and production deployment without explicit permissions.

## Continuous engineering loop within the same goal session
1. Use `git status --short -b; git rev-parse HEAD` and `python .agents/skills/mt2009-autopilot/scripts/checkpoint.py audit --head <ACTUAL_SHA>` to reconcile task-vs-campaign state. Audit branch, untracked `.agents`, local/remote divergence and any uncommitted changes. Never overwrite existing work.
2. Choose a **small, high-value, independently verifiable next step** consistent with existing goals. A failed test takes priority; otherwise choose the closest proof to playability. Do not repeatedly open a verified milestone or invent new project tracking documents.
3. Route the task through necessary specialist SKILL.md files; establish numeric sizes/offsets, Python 64-bit boundaries, security invariants, performance impact and minimum acceptance evidence as applicable.
4. Apply a narrow change, use isolated branch for high-risk Python/runtime refactors, protect `main` and other collaborators, preserve normal/preapproved Git constraints. No destructive reset, force push, unauthorized CI or live server access.
5. Run project verification **at the changed HEAD**: Windows GCC server build + tests where relevant; client MSVC C++20 syntax + actual AMD64 `.obj` checks where possible; FreeBSD GCC14 CI only with explicit bounded trigger approval and after local gates. **Windows build + its child processes** <=50% CPU via existing `tests/run_limited.ps1` and `AGENTS.md` current rules.
6. Diagnose failures, fix the real root cause, rerun impacted gates, self-review git diff and high-risk boundary tests. For economy/trade/offline shop, test negative/zero/near-limit YANG, replay, duplicate transactions, packet lengths and lifetime, with no unmeasured claims.
7. Record **one scoped gate at a time** using `checkpoint.py update --gate <gate> --status ... --head ... --verified-head ...`. `--status PASS` for a workstream cannot mark the campaign complete. Only use `--gate freebsd_ci --status PASS` with an exact run ID and matching verified HEAD.
8. Commit/push only within preauthorized branch and safety policy, once locally validated; never assume remote CI is green if the new SHA was never run. Update existing PROJECT_STATE.md for substantiated changes; keep checkpoint git-ignored and free of secrets.
9. **CONTINUE to the next independent safe task in the same active `/goal`.** If CI is in progress, mark `PENDING`; do not `gh run watch`, poll forever, or create recursive schedules. Use a one-shot check when allowed or work locally on an unrelated module. Report a short internal milestone, **not a request to copy the report into ChatGPT**.
10. End the campaign only when ALL original criteria are met, every remaining independently actionable step is genuinely blocked, or session/tool/quota constraints force termination. If forced, save a restartable checkpoint with next action and reason. No promise of automatic restart after a platform error or app closure.

## Initial reconciliation for the November 1, 2026 checkpoint (verify HEAD)
- Prior FreeBSD CI **Run 41311852071** succeeded on SHA `f58d9ce21510a1edba8de8f7daee91f10fa0195d`; it covered ShopEx and 168 wire assertions at THAT SHA, not the later protocol work.
- Subsequent development SHA `441ac569034447bf36467de55f1b993a3db708bc` changed 27 files (+803/-458), including server `exchange.cpp`, `input_main.cpp`, `special_shop_manager.cpp`, `ikarus_shop_manager.cpp`, client `Packet.h`/exchange/offline-shop and tests. Reported Windows wire `171/171`, CTest `10/10`, Lua `11/11`, quest `202/202`, and MSVC x64 `/Zs` `14/14`. These reports do not establish a FreeBSD result on `441ac569...` or a native PE32+ linked client.
- FIRST: reconcile actual HEAD and explicitly mark `freebsd_ci` at the *latest server-changing SHA* `UNVERIFIED` unless a matching newer run exists. Review the larger 27-file commit for regressions; add negative/malformed exchange/offline-shop tests; locally compile/test. Request a **bounded explicit CI trigger** if not already permitted, do not pretend an old CI verifies new code.
- NEXT: move toward actual client compilation **beyond `/Zs`** with MSVC `/c` and AMD64 `dumpbin /headers`; isolate missing i386 libs. Advance independent Python3 x64 + C++20 compatibility in isolated, testable slices. Do not do a mass blind Python2-to-3 replacement or touch deferred Granny.
- Open blockers do not mean that all engineering work is blocked. Resume from checkpoint, not from the original 2026-10-29 goal text as a fresh task list.

## Checkpoint contract
- Script is standard library only and does no Git/network actions. It uses `scratch/autopilot/state.json`, preserves v1 history, upgrades to v2 on `init`, and atomically saves. Verify scratch is Git-ignored before use.
- `update --gate freebsd_ci --status PASS` requires both `--run-id` and `--verified-head`; gate passes refer to their precise verified code SHA. `audit --head <sha>` highlights stale FreeBSD evidence. Never set overall campaign `COMPLETED` just because a checkpoint `current.status` says PASS.
- From repository root:
```
python .agents/skills/mt2009-autopilot/scripts/checkpoint.py init --goal "MT2009 native x64 autonomous campaign" --head <HEAD>
python .agents/skills/mt2009-autopilot/scripts/checkpoint.py audit --head <HEAD>
python .agents/skills/mt2009-autopilot/scripts/checkpoint.py update --gate freebsd_ci --task "ShopEx+LP64" --status PASS --head f58d9ce21510a1edba8de8f7daee91f10fa0195d --verified-head f58d9ce21510a1edba8de8f7daee91f10fa0195d --run-id 41311852071 --evidence "FreeBSD 15.1 GCC14 CI completed success"
python .agents/skills/mt2009-autopilot/scripts/checkpoint.py update --gate freebsd_ci --task "New Exchange/Ikarus commit" --status UNVERIFIED --head 441ac569034447bf36467de55f1b993a3db708bc --next "Run FreeBSD CI on this exact code after permission and local PASS"
```
- Clear `PASS`, `FAILED`, `PENDING`, `BLOCKED`, `UNVERIFIED`, `NOT_RUN` vs `ACTIVE` labels; no fake testing. A passed old run remains historical evidence but is not a guarantee for new source.

## Stop conditions and boundaries
- No production FreeBSD access, deployment, database writes, live service restart, SSH or credential extraction. No gratuitous documentation, framework or unrelated changes. No licensed binary piracy, no x86 masquerading, no fake stubs. If need for user decision arises, stop **only that decision** and work on other independent authorized tasks.
- A platform crash (e.g. prior `Agent execution terminated due to error`) is not proof the goal was achieved; preserve checkpoint and resume manually when tool access is restored. Continuous hours of progress are bounded by the Antigravity runtime and quota, not the SKILL.md text.
