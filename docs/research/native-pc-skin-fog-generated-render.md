# PC owned Fog/material/bone + SAN/scene/mesh → first shader generation (CP80)

2026-09-07; same pinned pristine PC EXE as [CP79](native-pc-skin-fog-render.md).
Те же три decoded references из whole491170, real bbush SAN/actor/scene
world, actual mesh429BC0, затем весь46A240 включая Fog4AD390, shader cache
miss4C8980→4CFFE0→SDK→4AF940→device4CA030→cache4C87A0→constants/draw.
Material Skin20 одновременно предоставляет fallback layers; это явный
renderer input. Выбор над отдельным fallback уже отдельно подтверждён CP76/77/79.

**5 exact captures,395 native assertions** (каждый70 linked+9 render):
linear/exp2/unknown/failed-device/post-false. Read74022,render42422..42569,
peak65328 bytes,121 engine owner generations полностью освобождены. Все
SDK/COM outputs и shader handle release также сверены. CP75 regression4/4
успешна после объединения source capture wrappers.

Fixture allocator получил явный вариант alignment8 (прежний default16
сохранён) и best-fit с начала данного графа. Все native objects остаются
в той же64KiB arena; live blocks не перемещаются, raw stream/RTTI/manager
inputs завершают lifetime только после фактического read/clear. Это политика
размещения внешнего allocator fixture, не восстановленный original malloc.

Предварительные варианты с отдельным вторым fallback material/shared pass
останавливались на fragmented272/353-byte string requests; состояния не
продолжались. Final fixture использует прежний тот же material как fallback,
без дополнительного owning pass. Ограничения100000 instructions/2s call,
30s child,32KiB allocation request и64KiB arena не увеличены.

Границы прежние: synthetic bounded SMO graph, actual setter для отдельного
mesh, prepared scene view/template, opaque SDK bytecode/COM, разделённые
завершённые фазы owner lifetimes. Это не whole asset/game/GPU execution.

```powershell
python research/native_workbench.py run pc-skin-fog-generated-render --deadline-utc 2026-09-07T16:00:00Z
```
