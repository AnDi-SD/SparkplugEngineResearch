# PC complete cached weighted submission

Source extends existing `SubmitUnlitGeometryForAnalysis` with optional explicit
constant inputs, using common `DrawCachedAutomaticForAnalysis`. No duplicate
alternate pipeline. Material identity must match its constant input; no-weight
callers retain existing API/default behavior. Generating miss returns incomplete.
Lit state, unknown/non-NULL texture resolution and preselected full submission
remain refused by this wrapper, not silently simulated as success.

PC DXRenderer76→78; no PS2 credit or class/gate closure. Remaining lit/
textured/whole-scene integration, descriptor/compiler production, full renderer
lifetime/reset, source mesh adapter and real GPU display still required.
