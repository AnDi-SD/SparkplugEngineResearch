# PC прочитанный Skin → generating shader miss → draw

Сохранённый Skin/Node/bone arrays из полного491170 передаётся в46A240 при
пустом shader cache. Внутри одного завершённого original render:

```text
palette/world → DXMesh4BC670 → material/pass4BC4A0 → automatic4BC290
→ manager4C8980 miss → shader factory4C9F10 → template4CFFE0
→ external SDK request/result → parameter append4AF940
→ device creation4CA030 → cache insert4C87A0 → constants4AE930 → draw4BE210
```

`SubmitUnlitGeometryForAnalysis` получил optional generation context и
использует существующий `DrawAutomaticForAnalysis`. Его передаёт
`SkinRenderContextForAnalysis::shaderGeneration`. Без контекста сохраняется
прежний cache-only контракт. Соединены прежние template/compiler/cache/
material/palette/draw части, второго алгоритма не добавлено.

## Память и уточнение атрибуции

Открыты whole SMO mesh/material acquisition, simultaneous SAN/frame,
lit/custom/fog/texture branches, библиотека SDK и живой GPU. Geometry buffers,
fallback material и renderer backing по-прежнему явные inputs.
