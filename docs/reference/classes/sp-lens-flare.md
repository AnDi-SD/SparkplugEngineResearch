# spLensFlare

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spLensFlare](../../../Sparkplug/Code/Sparkplug/spLensFlare.h), [spQuad](../../../Sparkplug/Code/Sparkplug/spQuad.h).

`spLensFlare` (`0x435370B5`) наследует `spRenderable`. Исторический структурный
анализ охватил шесть объектов (2/2/2); это не новый whole-graph regression.
Уточнённая собственная секция:

| Field | Payload |
| ---: | --- |
| 0 | primary element: material relationship + ARGB + `Single relativeDistance, scale`; повторная запись заменяет значение, NULL material сохраняет его |
| 1 | `UInt32 count`, затем count записей material relationship + ARGB + `Single relativeDistance, scale` |
| 2 | `Single occlusionSphereRadius, occlusionSpeed` |
| 3 | relationship на `spRenderNode` |

Field names, compound member getters и нулевой count независимо подтверждены
PC и PS2 executable. Новый Viewer projector требует actual loaded graph;
оба реальных уровня пока требуют OcclusionVolume. Запись остаётся
координированной задачей вместе с material и render-node ownership.
