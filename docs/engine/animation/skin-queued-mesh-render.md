# PC: queued Skin draw and mesh bounds

## Bounds producer

Original RenderMesh initialization424360 calls backend4AA000, then424230,
then marks bounds valid at mesh28.424230 calls468370/468000 for the sphere
and separately scans the index stream for AABB. The helper alone preserves
the prior valid flag. Sphere extrema use every vertex from the CPU vertex
buffer; midpoint/extent reconstruction has observable intermediate float
stores. Radius is the square root of the largest float-stored squared
distance, accumulated in x87 order Z²+Y²+X².

## Whole queued draw

Whole46A240 first enqueues via423FD0/454C30 without callbacks or GPU calls.
Whole454850 then calls actual4248D0, actual queued46A240, cached shader
selection, constants and4BE210 draw. External qsort receives a single
record and the original454800 comparator. A read-only instruction observer
at45489E records the genuine inner return without modifying registers.
Normal/negative device HRESULT/post-false all match queue record, ordering,
support publication, raw SAN state, world/palette/constants, material,
serialized buffers, device events and teardown. Queue returns1 and clears
count/flushing even when the genuine Skin post callback returns0; in that
case the Skin active-bone count remains1, as in the direct caller.

Reports:
