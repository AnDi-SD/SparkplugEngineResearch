# PC actual selected light cache through complete Skin draw

Source spLight scene refresh targets include that source RenderNode's cache.
New spDXRenderer::ResolveLightCacheForAnalysis reads the selected actual
spDXLight objects into stable caller-owned submission storage, preserving
ambient separation, ordinary order, raw type, enabled, color and payload.
The same sourceObject feeds shader constants. Unknown device words and identity
mapping are explicit caller boundary inputs; missing/non-DX host objects fail.
The helper does not treat stale unused slots as selected ordinary lights.
