# PC decoded texture + whole first shader generation (CP82)

2026-09-07; тот же pinned pristine PC executable и decoded graph, что
[CP81](native-pc-skin-texture-render.md). Native mip raw/DXT1/DXT3/DXT5,
Fog/material/bone из whole Skin reader, реальный bbush SAN, scene world,
decoded mesh, затем весь46A240 и cache miss→4CFFE0→SDK/reflection→device
creation→cache→constants→indexed draw.

**6 exact captures /522 native assertions** (78 linked+9 render каждый):
четыре формата, failed-device, post-false. TextureRead8703..8706,
render42711..42714, peak65416 bytes,130 engine owner generations freed.
COM texture/surface/device и opaque SDK/shader handle ownership сверены.
Ни один numerical capture не использует tolerance.

Ускоряющий приём: после завершения mesh reader вызван actual4AE140 для
renderer declaration lookup mapF358. Он освобождает tree entries/sentinel,
обнуляет F35C/F360, сохраняя declaration и его COM resource. Тот же decoded
mesh успешно используется до конца draw; declaration освобождён отдельно
при штатной очистке. Source ClearVertexDeclarationsForAnalysis переносит
очистку map; shared ownership сохраняет declaration, удерживаемый mesh.

Два предварительных размещения при ещё живом lookup map упёрлись в
fragmented353-byte string request:64752 reserved, самый большой free304.
Перестановка создания Texture/внешнего manager не изменила результат.
Такие состояния не продолжались; рабочий вариант добавляет подтверждённый
оригинальный destructor после последнего использования таблицы. Нет live
relocation, расширения arena или внутренних engine seams. Прежние пределы
64KiB/32KiB request/100000 instructions/2s/30s child сохранены.

Prepared scene/RTTI/template, external COM/SDK и отдельные завершённые
периоды read/SAN/world/render по-прежнему явны. Texture присоединена actual
setter после отдельного whole reader. Whole SMO texture/mesh acquisition,
реальный D3D/GPU и executable/game startup этим не восстановлены.

```powershell
python research/native_workbench.py run pc-skin-texture-generated-render --deadline-utc 2026-09-07T16:00:00Z
```
