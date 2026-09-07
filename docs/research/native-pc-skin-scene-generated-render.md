# PC SAN → scene world → Skin shader generation/draw (CP72)

2026-09-07; pristine EXE SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Продолжение [CP70](native-pc-skin-san-generated-render.md) и
[CP71 source manager](native-pc-scene-manager-world-source.md).

Из связки удалён отдельный test-side вызов bone421420. После actual
SAN manager/actor tick создаются original SceneManager45ADF0 и plain
Node421E20. У root position=(10,-20,30), затем original421A60 присоединяет
тот же decoded animated bone. Пока manager list пуст, scene membership
не назначается. После этого стенд предоставляет borrowed scene view14=root
и один list entry — это объявленный вход, не spScene constructor/initialize.

Единственный world entry стенда — **45A7D0**. Внутри него original421420
посещает root, затем ту же SAN bone; root argument0, child inherited461313,
currentScene сохраняется на обоих вызовах и очищается после обхода.
Смещение родителя реально добавляется к sampled local position. Original
parent и manager затем уничтожаются; borrowed bone/его world cache остаются.
Эти же bytes идут в Skin palette → первый shader miss → constants/draw.

Source harness вызывает новый spSceneManager, существующий plain Node
Attach и world traversal; специальных формул или подстановки world нет.
Четыре сценария quarter/three-quarter/loop/failed-device совпали **побитово**
по полному CP70 capture, теперь с parent-translated worlds/palette/constants.

**4 exact captures,189 native assertions**: по38+9 для обычных,39+9 для loop.
Skin read50702,SAN read39170,tick≤4857,scene world373,render42407 instructions.
Пик65440 bytes в тех же64KiB,98 owner generations all freed. Full build и
**CTest59/59,45,76с**, включая новый SceneManager42/42, успешны.

Это всё ещё завершённые фазы с освобождением SAN/scene infrastructure перед
renderer storage; full simultaneous frame не утверждается. Полный spScene
ctor/typed membership, whole SMO mesh/material acquisition и GPU остаются
отдельными обязательствами. Template/geometry/SDK outputs имеют прежние
явные границы. Предыдущие score сохранены: композиция сама не прибавляет
покрытие уже подтверждённым классам.

```powershell
python research/native_workbench.py run pc-skin-scene-generated-render --deadline-utc 2026-09-07T16:00:00Z
```
