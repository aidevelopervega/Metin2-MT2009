---
name: mt2009-protocol-abi
description: "Audits Metin2 MT2009 Windows x64 client vs FreeBSD amd64 server wire ABI: sizeof, offsetof, packing, packet headers and dynamic payloads, 64-bit PID AID guild IDs and YANG. Use during protocol compatibility fixes and packet regression reviews."
---
# MT2009 protocol ABI
## Procedure
1. Identify actual server producer and receiver, client producer and receiver, packet_info registration, dynamic frame sizes, and live feature flags before editing structs.
2. Build a two-sided contract: header and subheader numeric values, pack alignment, field type/width/signedness, sizeof, offsetof, array counts, trailing payload, endian assumptions and sequence/encryption framing.
3. Check Win64 LLP64 (long=4, pointer=8) versus FreeBSD amd64 platform model. Prefer fixed-width wire types without changing unaffected DWORD/VNUM/VID/slot fields.
4. Trace values beyond the wire: C++ model -> Python 2.7 PyInt/PyLong -> UI -> outbound packet and server authority. Pay attention to YANG/int64, UINT64 identifiers, UINT32 item count and overflow/negative cases.
5. Verify actual client header via its MSVC x64/x86 compile and static_assert separately from server-only GCC x64 wire tests. Use existing tests; avoid including conflicting client and server headers in the same translation unit unless namespace isolation is intentional.
6. Include extreme values: 0, -1 when invalid, 2^32+1, 2^53+1, UINT64_MAX, GOLD_MAX when relevant, invalid enum/window/slot and truncated packet stream.
## Python 3 migration coordination
- If modern Python bindings are part of the task, invoke `/mt2009-python3-cpp20-migration`. Maintain unchanged server wire ABI while mapping `PyInt`/`PyLong` to checked Python 3 `PyLong` and explicitly distinguishing Unicode text from binary packet bytes. Verify with actual CPython 3 C API tests (not arithmetic stand-ins).

## Acceptance gate
- Binary contracts match exactly for the active configuration; handler reads exactly its registered frame size.
- No unvalidated narrowing or changed server wire layout; Python APIs return large values exactly.
- /Zs and isolated packet assertions never count as live native client success.

## Project safety contract
- Scope: D:\mt22009; Windows native x64 client with a FreeBSD amd64 server. Inspect current branch, HEAD and worktree before changes. Never assume a historical report reflects current code.
- Default to read-only investigation and a narrow plan. Make code edits, branch changes, commits and pushes only if the current task explicitly authorizes each action. Never merge, deploy, restart production, change DB/schema, or alter a running FreeBSD server without explicit separate authorization.
- Granny source upgrade is deferred. Do not modify Granny, other third-party SDKs, external binaries or vendor licenses as a side effect.
- Keep all build/test child processes within the project's <=50% total CPU hard cap. Verify the existing tests/run_limited.ps1 syntax and actual enforcement first; if you cannot guarantee the cap, stop instead of running unrestricted builds/tests.
- Preserve wire compatibility and intended server behavior. Do not perform broad data-type replacements, speculative version changes or silent protocol edits.
- Distinguish factual code inspection, /Zs syntax+static_assert validation, object compilation, successful x64 linking, and live gameplay tests. Never use one as evidence for another.
- Report evidence with file:line, exact commands and exit codes, reproducible cases, impact, remaining BLOCKED/UNVERIFIED items. No fake PASS; changes are not complete until relevant tests really ran.
