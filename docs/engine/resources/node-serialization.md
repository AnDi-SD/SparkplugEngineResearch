# PC Node serialization: fields → relationships → owned tree

## Common reconstructed core

`spNodeSerializer` now implements bounded sections/read/write/index via the
same `spSerializer` registry, reference framing and FAT used for SAN/meshes.
Protected section helpers allow derived serializers to consume sequential
base/derived sections without a second import implementation. Until a derived
adapter exists, strict public target guards prohibit silently saving only its
Node base. Collision payloads explicitly fail, not disappear.

Read context uses canonical `shared_ptr` owners. Created objects share their
owner with child edges; prebound/cache objects require an explicit external
owner. No invented no-op deleter is used. Holding the root past context teardown
keeps its child alive; dropping it releases the represented tree. This is host
ownership, not a proof of every native intrusive rollback or scene lifetime.

Different-parent reparent is still rejected; native scene registration/collisions, graph writer depth and unknown-field lossless storage remain open. Partial mutation on later error is not promised transactional.

Billboard integration includes NULL identity, valid camera and degenerate rejection.
