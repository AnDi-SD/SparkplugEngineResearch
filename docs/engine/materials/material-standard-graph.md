# PC standard material graph and DX runtime identity

## Standard-layer field grammar and ownership

Ownership is not uniformly intrusive. Material retains Pass(ref1), but Pass
directly deletes Layer and Layer directly deletes MaterialTexture(ref0,0).
45F5E0 replaces/deletes a layer, then sets count=max(count,index+1), **even
forNULL**. It does not shift/shrink like Material423960. 45F5B0 clears all
eight slots regardless of count. Source now uses unique owners for these two
direct-delete edges and shared canonical owners for the Material->Pass edge.
Same-pointer destructive replacement and out-of-bounds native calls not run.

An early UV input omitted the flag (36 bytes): native returned true with a
read diagnostic and consumed across field boundaries. Correct valid inputs
are40 bytes. Safe host rejects the malformed field before mutation; native
malformed acceptance is not presented as a safe parsing contract.

## Connected Model graph

Four modes: inline, repeated same reference, clear viaNULL, prebound. Real
Model4938F0 -> Renderable -> common4678B0 -> header/factory -> Material fields
loads the graph; actual recursive index4672C0 and common reference writer
write nested output. Inline/repeat create DXMaterial; prebound MaterialData
stays MaterialData because factory/payload are skipped. Pass/Layer inline
objects do not acquire independent FAT entries. Explicit Color field in the
valid graph initializes DX power; earlier scout output containing poison
is not used as a portable golden file.

Native clear deletes the sole-owned material but FAT retains its stale
address; never reused/dereferenced after deletion. Portable read context
keeps a canonical owner alive, an explicit safe lifetime difference.
