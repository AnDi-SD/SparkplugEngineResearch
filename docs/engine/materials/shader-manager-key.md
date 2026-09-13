# PC shader manager and renderer key

## Manager lifetime and cache

Actual protected4C9680 factory succeeds under unchanged limits: size0x50,
primary6F2DE4 and secondary+10=6F2DE0. Empty vectors18/1C/20 and28/2C/30,
map root38/count3C, NULL template40, pair-key map root48/count4C.
Allocator words14/24/34/44 are untouched, not fabricated pointer defaults.
Primary slots:4C9660,5B7A00,4C96E0,40ECE0,4C9480,408350,408370,
4C8980,4C97E0,4C8F10. The next words are a string, not more virtual slots.

Zero returnsNULL without even looking at the cache, including when an entry for that key exists. Otherwise4C80F0 finds the pair. Node+14 is the returned shader pointer. Cache miss enters source generation/compilation; it is NOT a normal NULL hit. The source `SelectCachedForAnalysis` explicitly returns completed=false on such a miss. No internal compilation function is replaced by fake success.

Clone4C96E0 registers an actual clone and calls40ECE0: new **empty** manager,
not a copy of cache/template/vectors.4C9370 destroys template40 if present,
direct-deletes each non-NULL cache value, destroys nodes/root, then4B0740
destroys both owned shader vectors and base map.4B0740 unconditionally
clears763024, even if its value no longer equals this manager. Portable
source deliberately has no process-global renderer/manager singleton;
that lifetime side effect is documented, not claimed as implemented.
Populated first/second vectors, template object and base name-map ownership
still need independent execution; cache destruction is fully exercised.

## Renderer key, original4BE310

The bounded prefix executes through original material power getter and actual
manager factory, stopping **before** indirect call4BE495, never resuming it.
Separate no-weight cases execute the whole4BE310→4C8980 and returnNULL.
Native source-generation format strings give real parameter names:

| Key portion | Original meaning and construction |
| --- | --- |
| bits0..3 | BlendWeightCount: highest component bit10/8/4/2 gives4/3/2/1 |
| bits4..7 | texture-coordinate count: highest component bit40000..800 gives8..1 |
| bits8..15 | bHasUVTransform[0..7]: raw state8 atC8B8+24hex*stage has bit2 |
| bits16..19 | ColorMode; construction actually ORs **raw C888<<16**, no mask |
| bits20..23 | LightCount from lightsC190+24, shifted20, no count mask |
| bit24 | bUseSpecular iff selected materialE47C adjusted+14 getter+28 returns >0 |
| second word | OR raw light+C0 types shifted2*i, without masking each type |

Zero/negative/NaN power clear specular; positive subnormal and +infinity set
it. Raw out-of-range color/type inputs preserve overlapping bits exactly.
Source limits explicit light arrays to8; native has no such loop guard.
These are consumer inputs, not a claim that renderer startup permits all
raw combinations. No PC observations are transferred to PS2.
