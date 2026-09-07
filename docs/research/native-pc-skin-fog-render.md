# PC Skin-owned Fog/material/bone → SAN/scene/mesh draw (CP79)

2026-09-07; pinned pristine executable as [CP78](native-pc-renderer-fog.md).
Whole491170 читает три FAT references: Node7,Material8,Fog9. Renderable
field1 вызывает actual4678B0→43B910→423D20; Skin24 получает один owning
Fog ref. Prepared RTTI имеет четыре entries: Std,MaterialData,Node,Fog;
проверки membership/factory/resolver остаются original.

После actual SAN actor/scene sampling и mesh reader тот же Fog проходит
423FD0→4AD390→4B0A90 перед Skin world transform/palette/draw. Все Fog device
events видят прежний active bone count9 и ещё не bound geometry. Unknown
Fog type4 возвращает false из setter, но original pre игнорирует результат:
Skin продолжает draw и успешно завершается. Post false сохраняет count1.
Материал и Fog освобождаются actual Skin destructor, массив bone references
остаётся отдельным borrowed native контрактом.

**9 exact captures,675 native assertions** (каждый66 linked+9 render):
disabled/exp/exp2/linear/unknown/raw-linear/raw-density/failed-device/post-false.
Read74022,render15303..15450,peak64704 bytes,110 owner generations released.
Raw Fog payload/device cache, material, mesh, SAN/scene PRS и весь draw trace
совпали с C++. CP77 regression5/5 успешна. Source Skin теперь вызывает
перенесённый ApplyFog после material selection и тоже игнорирует его
native false; неверный host object type и missing inputs guarded отдельно.

Граф bounded/synthetic; реальная SAN bbush прежняя. Model mesh reference
по-прежнему составлена actual setter после отдельного mesh read; scene view
prepared, owner lifetimes разделены на завершённые фазы. Shader здесь cached,
не генерируется. Ни GPU/OS, ни caps/arena не менялись.

```powershell
python research/native_workbench.py run pc-skin-fog-render --deadline-utc 2026-09-07T16:00:00Z
```
