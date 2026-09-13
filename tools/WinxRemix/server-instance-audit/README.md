# Server instance audit

Optional diagnostics in our Remix bridge. Apply `instance-audit-v1.patch` after the camera patch on the base revision in [manifest.json](manifest.json).

## What is counted

The existing single device-command queue receives both Remix instance commands and D3D commands. Immediately after each real `g_remix.DrawInstance` returns, the audit counts success, error and whether the bridge mesh-handle lookup was valid. No extra renderer calls or synchronous ACKs are introduced. A call that never returns is not counted.

Success means the **renderer API returned SUCCESS after accepting work through EmitCs**. It does not prove GPU completion, eventual scene retention or a visible instance. Client-side DrawInstance success alone does not provide this server evidence.

Each interval records the first ordinary draw command on the registered device: DrawPrimitive, DrawIndexedPrimitive, DrawPrimitiveUP or DrawIndexedPrimitiveUP (kind 1–4). Recording occurs before the original call, including zero primitive counts and calls which subsequently fail. `early*` counts API returns before that first command. API calls on unknown registration state remain counted, but cannot qualify order.

Both existing Present variants close intervals **after the real Present returns**. The main client Device::Present actually delegates to its implicit swapchain, so the swapchain hook is necessary. The audit links this identity from the existing successful LinkSwapchain/GetSwapChain(0) result; it adds no COM queries, reads or references. It handles the alternate Device::Present path as well.

`complete` means a matching Present command returned; it does not imply success. `qualifiedOrder` additionally requires successful registration, no foreign-device draw/Present ambiguity and a nonfailed Present result. `interval`, `presentIndex` and `commandSequence` are audit counters, not native/game/GPU frame IDs. Command sequence is independent of UID reuse/wrap. A qualified interval with no ordinary draw has all its API calls early; a visible-world ordering claim should also require ordinaryDrawCommands > 0.

For A/B analysis, compare complete qualified interval rows in equivalent gameplay periods, and retain their exact counts. Global cumulative early totals include incomplete and unqualified intervals and must not be used by themselves as proof of per-frame ordering.

Here “first ordinary draw” means the first ordinary draw **remaining in the server command stream**. A legacy adapter's DrawInstance at a client-side D3D interception point can also count as early when that adapter suppresses the original D3D call. The audit has no client-lane tag. Therefore expected A=725/B=0 is a test hypothesis, not an invariant or attribution proof: correlate client lane counts/sequence and a controlled A/B before assigning early server calls to the independent scene exporter.

Reset, registered-device destruction, implicit-swapchain destruction/replacement, registration replacement and device-queue exit emit incomplete intervals. Pre-registration API errors and foreign events are closed by `registration_boundary` rather than discarded. Reset keeps only the existing bridge swapchain mapping metadata; a failed Reset disqualifies the following interval. Device/swapchain destruction invalidates their metadata. No raw object identity is dereferenced or retained with AddRef.

## Enabling and bounds

Set `REMIX_BRIDGE_INSTANCE_AUDIT` in the launcher environment to a **fresh absolute JSONL path** before the existing bridge process is created. Its parent directory must already exist. The existing CreateProcess inherits the parent environment. The file is opened with CREATE_NEW and FILE_SHARE_READ; existing files are never overwritten. Drive-absolute paths and ordinary UNC paths are accepted, device/extended namespace and relative paths are rejected. The launcher should use a local run directory.

With no usable environment path, audit is disabled. File size is bounded to 16 MiB; the writer reserves room for a `cap` marker, then stops auditing. Formatting, open, short-write or write errors disable the audit without altering bridge return values or transport state. A truncated/I/O-failed file cannot be called a complete run. Output is buffered by Windows normally; there is no per-instance write or flush, only interval/registration records. This bounds file growth, not storage latency.


Build and CPU-test scripts are stored beside the patch. Installation remains an explicit operation against the selected local runtime.
