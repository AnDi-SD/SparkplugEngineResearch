# PC real SAN/scene world + decoded mesh → Skin draw

Одни и те же decoded Skin/Node проходят real bbush SAN read, actual actor
dispatch, manager45A7D0→parent/bone world. После завершения SAN/scene owners
исполняется полный mesh429BC0, и тот же materialized DXMesh через479E20
передаётся в46A240, включая first shader generation, constants и draw.
Source использует прежние readers/actor/SceneManager/mesh/Skin алгоритмы.

Прежние границы открыты: чтения составлены через actual setter, не whole
SMO Model-reference load; scene view prepared, owner lifetimes разделены
на завершённые фазы. Material/template/SDK outputs пока inputs. Ни cap,
ни размер engine arena не увеличен, OS/GPU не вызываются.
