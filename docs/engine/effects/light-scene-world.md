# PC decoded light: whole Scene attachment and world cache refresh

Source spLight now holds an explicit borrowed SceneLightManager binding.
Its world clears8 and invokes that manager before the derived DX payload.
spLightManager retains explicit borrowed cache/sphere targets and applies the
same eligibility/add/remove algorithm in stable order. Register/unregister
guards and vector storage are host choices. Binding is not automatic native
Node attachment, a complete source Scene class, partition traversal or scene
render initialization. The native manager+1C render-list link is the explicit
45D850 assignment; the unrelated full SceneInit is not executed. Renderer
storage is zero caller input inside the same arena, without OS/GPU forwarding.

## Refresh flags and observed cache

The derived DXLight captures stored and inherited flags before calling the
base world operation. Bit `1` implies captured refresh bit `8`. After Node
world clears bits `1`, `2` and `4`, Light tests its remaining flags together
with inherited bit `8` and refreshes Scene.LightManager targets. DXLight then
refreshes its device payload when the captured refresh applies and its own
light-enabled byte is set. These are separate decisions.

| Change | Node world transform | Per-object scene light caches | DX device payload |
| --- | --- | --- | --- |
| Local dirty `1` | Updated | No refresh from local `1` alone | Updated if light enabled |
| Explicit or inherited `8` | Retained unless `1` also applies | Eligibility/add/remove refreshed | Updated if light enabled |
| Raw intensity/color without a marker | Retained | Retained | Retains old words |
| Disabled light with dirty `1` and `8` | Updated; dirty flags cleared | Light removed from eligible targets | Retains old words |

Per-object eligibility considers the light-enabled byte, active hierarchy,
projected-shadow policy and range versus the target's world sphere. It does
not define the loaded scene light registry. A far light can remain registered
and have an updated device payload while being absent from a target's cache.
Ordinary Node world traversal visits children without a camera-frustum gate;
its camera dependency is billboard orientation. In the
[base PC frame](../architecture/engine-frame.md), animation and Scene/world
updates precede graphics. Custom game overrides and complete live producer
scheduling require their own qualification.

Shared reference operations:
[Node world](../../../Sparkplug/Code/Sparkplug/spNode.cpp),
[Light world](../../../Sparkplug/Code/Sparkplug/spLight.cpp),
[DXLight world](../../../Sparkplug/Code/SparkplugDX/spDXLight.cpp) and
[payload arithmetic](../../../Sparkplug/Code/SparkplugDX/spPCLightPayload.h).
