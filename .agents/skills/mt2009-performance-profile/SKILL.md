---
name: mt2009-performance-profile
description: "Profiles MT2009 client FPS/frame time, CPU, memory, allocations, GPU/renderer costs, loading times and server/network ticks. Use to prioritize measured C++ optimization and avoid premature or compatibility-breaking refactoring."
---
# MT2009 performance profiling
## Procedure
1. Choose the test environment and workload: cold start, login/character select, crowded map, combat/effects, UI/inventory, network reconnect. Capture build SHA, graphics/settings, client/server machine and deterministic scene.
2. Define baseline metrics before optimization: frame time p50/p95/p99 and 1% lows (when a running client exists), FPS, per-process CPU, resident memory, heap allocation hot spots, loading time, network bytes/s, RTT and server tick/loop latency. Separate CPU time from wall time.
3. Select available safe profilers only after checking availability (VS Performance Profiler, Windows Performance Recorder, ETW, perf/pmc in isolated FreeBSD environment); do not install or enable invasive system agents without approval.
4. Locate proven bottlenecks, then modify one focused hotspot. Prefer algorithmic complexity, batch size, cache locality and unnecessary allocations only when the profile supports it.
5. Use before/after on the same test workload, reporting absolute and percentage deltas, variance and correctness checks. Revert regressions rather than asserting optimizations by intuition.
6. Keep profiling and benchmarking jobs under the <=50% total CPU cap; note that a cap changes absolute performance and compare only measurements under equivalent caps.
## Python 3 performance reality
- If CPython upgrade is proposed, coordinate with `/mt2009-python3-cpp20-migration`. Benchmark UI tick, startup, script import, frame-time p50/p95/p99, resident memory and CPU against a comparable baseline; do not claim automatically higher FPS or enable GIL-free builds without native extension safety validation.

## Acceptance gate
- Measured improvements and preserved behavior; no 'faster' claims without quantitative evidence.
- No claim about FPS or gameplay responsiveness before actual linked x64 client is run.

## Project safety contract
- Scope: D:\mt22009; Windows native x64 client with a FreeBSD amd64 server. Inspect current branch, HEAD and worktree before changes. Never assume a historical report reflects current code.
- Default to read-only investigation and a narrow plan. Make code edits, branch changes, commits and pushes only if the current task explicitly authorizes each action. Never merge, deploy, restart production, change DB/schema, or alter a running FreeBSD server without explicit separate authorization.
- Granny source upgrade is deferred. Do not modify Granny, other third-party SDKs, external binaries or vendor licenses as a side effect.
- Keep all build/test child processes within the project's <=50% total CPU hard cap. Verify the existing tests/run_limited.ps1 syntax and actual enforcement first; if you cannot guarantee the cap, stop instead of running unrestricted builds/tests.
- Preserve wire compatibility and intended server behavior. Do not perform broad data-type replacements, speculative version changes or silent protocol edits.
- Distinguish factual code inspection, /Zs syntax+static_assert validation, object compilation, successful x64 linking, and live gameplay tests. Never use one as evidence for another.
- Report evidence with file:line, exact commands and exit codes, reproducible cases, impact, remaining BLOCKED/UNVERIFIED items. No fake PASS; changes are not complete until relevant tests really ran.
