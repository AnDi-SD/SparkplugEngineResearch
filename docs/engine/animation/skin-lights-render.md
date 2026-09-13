# PC Skin: complete light submission in decoded draw

## Connected path

Cases:NULL list, empty list, directional/point/spot, disabled ordinary,
ambient-only, negative device HRESULT, false post callback, and all raw
material lighting modes0..7 (unlit-list covers2). Source and original agree
on complete device events, raw material colors, group/cache/count state,
light payload, SAN/scene palette, mesh bytes, shader key and draw output.
Render15315..15713 instructions,peak64312 bytes,99 freed engine owner
generations. Original light world producer/scene light selection are still
prepared inputs; this is not a full Scene frame or light asset loader.

Disabled ordinary lights still contribute their count/type to4BE310's
shader key. A non-NULL light list also contributes when raw material mode2
skips4BDE50 entirely. NULL submission disables8 slots and preserves old
global count/group pointers; an empty non-NULL list clears both. Existing
common SubmitLights logic now runs inside source whole geometry submission.

## Two source gaps found by composition

Validation:
