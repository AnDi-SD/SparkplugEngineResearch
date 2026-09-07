# PC Skin-owned material + SAN/scene/mesh → draw (CP76)

2026-09-07; pinned pristine PC EXE SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Продолжение [CP75](native-pc-skin-material-mesh-render.md).

Два FAT entries: Node ID7/class695C0F65 и Material ID8/class6160348B.
Whole491170 теперь читает Renderable field0 с inline MaterialData body,
затем Model terminator и Skin field0 с bone reference/inverse bind.
Actual47FBA0→4678B0→42F4C0/42F670 создаёт DXMaterial, pass и два StdLayer.
Reader инкрементирует material+8 и записывает Skin20; после завершения
serializer/FAT lifetimes материал остаётся жив с одним owning ref.
Трёхэлементный RTTI input объявлен явно, original membership/factory работают.

Те же Skin/Node/material проходят real bbush SAN, actor dispatch, SceneManager
world, actual mesh reader и46A240. Fallback — отдельный белый DXMaterial;
423FD0 выбирает цветной Skin20 в rendererC18C. Native4BE405 включает bit24
shader key при power3.5: key01020011, а не00020011. Shader cache подготовлен
через actual4AF940/4C87A0; draw выполняет cache hit. Ошибочные ранние
fixture keys приводили в неподготовленную generation branch; состояния
не продолжались, исправленный вход проверен в новом child.

**4 exact captures,281 native assertions**: quarter0.25,three-quarter0.75,
loop1.25,failed-device0.5. По61 linked+9 render, loop62+9. SkinRead70998,
meshRead25186,render15298; peak64656 bytes,103 engine owner generations
полностью освобождены. Skin destructor сам освобождает decoded material.
CP75 повторён4/4 после общего refactor. Source спускается через actual
Skin/MaterialData readers; RenderUnlit теперь допускает собственный unlit
DXMaterial с pass0/blend0 и overrideByte0, повторно читая ссылку после pre
callback. Неизвестные queued/override/fog ветки пока guarded.

Граница: Model mesh reference всё ещё составлена через actual479E20 после
отдельного mesh read. Это bounded двухссылочный граф, не whole asset SMO;
scene view prepared, owner lifetimes разделены на завершённые фазы.
Shader generation на этом графе не заявлена; отдельно доказана CP75.
Ни caps, ни 64KiB arena не увеличены; GPU/OS не вызываются.

```powershell
python research/native_workbench.py run pc-skin-owned-material-render --deadline-utc 2026-09-07T16:00:00Z
```
