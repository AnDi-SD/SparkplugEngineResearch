# PC actual selected light cache through complete Skin draw

CP93,2026-09-07.11 exact captures/954 native assertions: nine pinned unchanged
SMO lights plus failed-device/post-false on case6. Same CP91 SAN/scene-world/
decoded mesh/owned material chain, now with actual LightManager-produced cache.
Whole light world selection2226..2363 instructions,render15935..16175,
peak65072/65536 bytes,108 engine owners all released. Caps and EXE unchanged.

Actual425520 constructs a RenderNode with its embedded cacheF0; actual46AB80
constructs LightManager and45A780 registers the decoded light. The node's
current sphere is explicit at that light's world position with zero radius,
shadow exclusion is false, and actual420DE0 enables hierarchy100. A borrowed
Scene34 and manager1C head are declared inputs; manager20 owner isNULL.
Whole4B58D0→428C30→46ACE0/46A850/490B50 selects the same light into the actual
RenderNode cache. No cache slots are filled by the fixture. The exact nine
slots and count are captured. After ending the borrowed scene inputs, actual
manager destruction leaves this borrowed cache intact. Whole46A240 uses
that physical cache asC190, resolves key/constants and draws. The actual
RenderNode destructor later clears rendererC190 and frees its own storage.

Source spLight scene refresh targets include that source RenderNode's cache.
New spDXRenderer::ResolveLightCacheForAnalysis reads the selected actual
spDXLight objects into stable caller-owned submission storage, preserving
ambient separation, ordinary order, raw type, enabled, color and payload.
The same sourceObject feeds shader constants. Unknown device words and identity
mapping are explicit caller boundary inputs; missing/non-DX host objects fail.
The helper does not treat stale unused slots as selected ordinary lights.

This completes the selection-cache-to-renderer bridge for these fixtures.
The earlier CP92 separately executes actual Scene construction and attachment;
this tightly bounded draw fixture uses a prepared borrowed scene view and ends
that dependency before drawing. No complete live Scene frame, visibility,
partition, first lit generation, GPU, or PS2 claim follows from the bridge.

Report20260907T145105360996Z-pc-skin-selected-light.json11/11.
Full build/CTest61/61 passed after CP93 production changes in52.88s.
855 linked assertions plus99 common draw assertions yield954. Initial local
probe assertion read the destructor's reset visit map after a successful
producer; moving completed-manager teardown to the subsequent fixture phase
preserved the producer observation. No original method/result was replaced.
Cumulative CP51..93:336 exact/10583 native assertions, plus38 native-only CP88.
