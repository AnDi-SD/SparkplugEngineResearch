# PC decoded material + SAN/scene/mesh → Skin draw (CP75)

2026-09-07; pristine EXE SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Продолжение [CP74](native-pc-skin-scene-mesh-render.md): material теперь
создаёт actual reader42F4C0/42F670→4774D0, включая pass и два StdLayer
через original RTTI factory460E50. Тот же объект используется draw.

Wire содержит 11 render states, ARGB ambient/diffuse/specular/emissive,
power3.5, pass blend0, два StdLayer и девять texture states первого слоя.
Цвет diffuseFF804020, specularFF102030, emissive00112233, borderFF112233
и filter1 проходят в raw material capture, device trace и MatDiffuse
shader constant. Сверяются первые девять wire texture states; дополнительные
три source analytical slots не приписываются формату.

**4 exact captures,289 native assertions**: quarter0.25,three-quarter0.75,
loop1.25,failed-device0.5. По63 linked+9 render, у loop64+9.
MaterialRead14713,meshRead25186,render42408 instructions. Пик65424 bytes,
110 engine owner generations полностью освобождены. Предыдущий CP74
повторён4/4. Source использует actual spMaterialDataSerializer и прежний
Skin/renderer; production алгоритмы в этом checkpoint не менялись.

Материал пока передан в fallbackC9C0; Skin20 остаётся NULL. Это связь
decoded material→consumer, не whole SMO material-reference transaction.
Prepared shader template, external SDK/COM outputs, scene view и разделение
owner lifetimes на завершённые фазы сохраняются. Caps не увеличивались.

```powershell
python research/native_workbench.py run pc-skin-material-mesh-render --deadline-utc 2026-09-07T16:00:00Z
```
