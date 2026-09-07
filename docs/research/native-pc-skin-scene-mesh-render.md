# PC real SAN/scene world + decoded mesh → Skin draw (CP74)

2026-09-07; pristine EXE SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Композиция [CP72 scene/SAN](native-pc-skin-scene-generated-render.md) и
[CP73 mesh reader](native-pc-skin-mesh-generated-render.md).

Одни и те же decoded Skin/Node проходят real bbush SAN read, actual actor
dispatch, manager45A7D0→parent/bone world. После завершения SAN/scene owners
исполняется полный mesh429BC0, и тот же materialized DXMesh через479E20
передаётся в46A240, включая first shader generation, constants и draw.
Source использует прежние readers/actor/SceneManager/mesh/Skin алгоритмы.

**4 exact captures,261 native assertions**: quarter0.25,three-quarter0.75,
loop1.25,failed-device0.5. По56 linked+9 render, у loop57+9. Сверяются raw
SAN sample/PRS, mesh bytes/declaration/metadata и полный device trace.
SkinRead50702,SAN39170,tick≤4857,world373,meshRead25186,render42407.
Пик65424 bytes,107 owner generations полностью освобождены. CP73 2/2
повторены после выделения общего MeshRead fixture. Новых production
алгоритмов этот checkpoint не вводит; class scores сохраняются.

Прежние границы открыты: чтения составлены через actual setter, не whole
SMO Model-reference load; scene view prepared, owner lifetimes разделены
на завершённые фазы. Material/template/SDK outputs пока inputs. Ни cap,
ни размер engine arena не увеличен, OS/GPU не вызываются.

```powershell
python research/native_workbench.py run pc-skin-scene-mesh-render --deadline-utc 2026-09-07T16:00:00Z
```
