# PC preselected-shader draw and shader stacks

## Command and cache order

The selected vertex shader is compared with bound identityE44C. Changed
identity is stored **before** COM+170 receives its+50 handle. NonzeroE444
always submits that many float4 registers fromCBC4 to COM+178 at register0,
including repeats. The native buffer/count total extent is not guessed.

Only when byteF2F5 is nonzero, selected pixel shaderE464[E478] is compared
with boundE450, cached before COM+1AC. NULL binds handle0. Pixel constants
E448 rows atD804 submit to COM+1B4 only with a non-NULL bound pixel shader.
Disabling F2F5 does not clear a previously bound pixel identity.

All seven input words are kept positionally until original API names are
recovered. Let them be `(kind,a2,a3,a4,a5,a6,a7)`:

| Native branch | Exact device command arguments |
| --- | --- |
| kind1 | DrawPrimitive(1,a4,a3), COM144 |
| kind0/2/3/4 | DrawIndexedPrimitive(table[kind],a4,0,a5,a2,a3), COM148 |

Table6F1A54 bounded indices0..4 is `[1,1,4,5,2]`. Index1 uses the separate
nonindexed branch. a6 is consumed only by automatic shader selection, not
this branch; a7 unused here. Every HRESULT is ignored and native returnsAL1.
Source rejects kind>4 rather than reproducing an unsafe native table read.

## Stack lifetime is separate from binding

4BE1B0 incrementsE474 then stores vertex shader atE454[top]. Protected 4BE1D0 decrements top without clearing the old element. Pixel equivalents are4BE1E0/E478/E464 and4BE200. Helpers make no device calls/retain/release. Bounded push/push-NULL/pop preserves stale entries exactly. This does not establish safe arbitrary nesting.
