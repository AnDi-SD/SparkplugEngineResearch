# ParticleSystemManager update with an existing EngineCore (PC)

The PC frame routine calls the particle manager through virtual slot `+0x2C`. The slot in `spParticleSystemManager` primary `0x006E70F8` points to `0x0045A390`.

The method reads the sentinel pointer at manager `+0x1C` and starts at the sentinel's next link. An empty ring returns before reading the EngineCore global. For each entry, it reads the current EngineCore pointer at `0x00755274` and the four delta bytes at its `+0x7C`. It passes those bits to the particle update at `0x0048D450` with the borrowed particle pointer from node `+0x8`. It reads the current node's next link and the manager's sentinel after the update. Traversal uses the links and sentinel. Repeated particle pointers are dispatched once per list node.

The analysis facade takes a captured primary and a finite, coherent sentinel-ended view of live borrowed nodes. It obtains the existing EngineCore and raw delta separately for each update. A synchronous callback supplies the [particle update](particle-cpu-update.md) effect. Each current node stays live for the next-link read after its callback. The facade keeps the borrowed links and forwards delta bits intact. An empty list needs no callbacks. An unavailable core in a nonempty list makes the facade refuse, with any completed callbacks preserved.

The original can initialize a missing EngineCore on a nonempty list. That branch, the global write, native allocator and lifecycle, the gameplay-derived manager, PS2 behavior and full engine-frame operation remain open.
