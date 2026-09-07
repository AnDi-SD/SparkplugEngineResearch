# `spMaterialSerializer`: общая грамматика материала

PC checkpoint26: [color reference/index/writer and frame consumer](native-pc-material-color-graph.md).
CP19–26 add canonical fallback, AnimTex, UV and prebound ColorController edges
through the common resource core. Protected ColorController inline factory is
still unavailable; nonstandard layers and full pipeline are not closed.

Historical PC checkpoint13: [standard-layer graph and actual DX runtime identity](native-pc-material-standard-graph.md).
The common bounded codec now handles MaterialData/DXMaterial scalar fields,
StdLayer states8/17 and UV9, using common references and DataBlock framing.
Nonstandard layers and nonempty external texture/controller branches still
reject explicitly; this is not whole real material or renderer completeness.

Статус: identity/base, lifetime, встроенный data-block helper, стандартный layer-path,
reader field set и resource-index order подтверждены executable-кодом и массовым
SMO-корпусом. Редкие camera/movie/render-target/environment-map ветки пока остаются
evidence-only и не выдаются portable planner-ом за полностью восстановленные.

PC сохраняет точный путь translation unit:

`Z:\Sparkplug\Code\Sparkplug\spMaterialSerializer.cpp`

PS2 независимо сохраняет имя `spMaterialSerializer.cpp`.

## Идентичность и ABI

Class ID `0x2A14745F`, direct registered/C++ base — `spSerializer`
(`0x42429877`). Базовый класс владеет общей грамматикой, однако отдельный target-ID
hook у него не подтверждён: concrete `spMaterialDataSerializer` добавляет target позднее.

| Свойство | PC | PS2 |
|---|---:|---:|
| Registration | `0x00760998` | `0x004AA750` |
| Initializer | `0x006D3D00` | `0x00483690` |
| Factory | `0x00477F00` (protected entry) | `0x00195D30` |
| Constructor | protected path | `0x00195BF0` |
| Destructor | `0x00477480`, deleting `0x00477EE0` | deleting `0x00195B60` |
| Blank clone | `0x00477F60` | `0x00195C40` |
| Registration getter | `0x004774C0` | `0x001928F0` |
| Object extent | observed `0x3C` | exact allocation `0x3C` |
| data-block state | `+0x14`, observed extent `0x28` | `+0x14`, exact extent `0x28` |

PS2 constructor сначала строит `spSerializer`, ставит vtable headers
`0x0048F890/0x0048F8B4`, затем вызывает constructor data-block helper для `this+0x14`.
Destructor освобождает тот же helper до базового serializer. PC primary vtable наблюдается
по `0x006EA728`; защищённая factory entry не объявляется прямым доказательством `sizeof`.

## Методы и helpers

На PS2 точно отделены top-level writer `0x00193060`, reader `0x00193EB0` и resource
index pass `0x001959D0`; thunks находятся по `0x00195E50/0x00195E40/0x00195E30`.
Layer helpers: load `0x00192D90`, serialize `0x00195300`, index `0x001956A0`.

PC protected/obfuscated top-level entries не названы по предположению. Независимые
layer helpers видны по `0x00477230`, `0x00477350`, `0x004767F0`; error-string xrefs
подтверждают top-level writer/index/reader bodies внутри защищённой области.

## Подтверждённый стандартный write plan

Для обычного `spStdLayer` (`0x234C576B`) порядок следующий:

1. поле 1 `VertexAlpha`, только если значение true;
2. поле 0 `RenderStates`, всегда: 11 `uint32`;
3. для каждого pass поле 3 `Pass` (`FinalBlendOp`);
4. для каждого layer поле 4 с class ID, затем поле 17 с девятью texture states;
5. необязательное поле 9 static UV matrix;
6. необязательные relationships 10 texture, 11 animation controller, 12 UV controller;
7. поле 2 color: четыре ARGB и specular power;
8. поле 6 color-controller relationship, включая null.

Reader также принимает legacy texture-state поле 8 вместо текущего 17 и пропускает
неизвестные поля. `BuildStandardWritePlanForAnalysis` намеренно возвращает `valid=false`
для class ID не-`spStdLayer`: executable содержит дополнительные ветки, но их полный
field contract ещё не доказан.

Index pass идёт по отношениям каждого стандартного layer в порядке texture,
UV-controller, animation-controller, затем индексирует material color controller.

Runtime counterparts этой wire-грамматики теперь независимо подтверждены:
`spMaterialPassLayer` содержит blend/count и восемь layer pointers, а
`spStdLayer` владеет отдельным `spMaterialTexture` с texture states и UV/
controller relationships. См.
[`native-class-sp-material-layers.md`](native-class-sp-material-layers.md).

## Сверка с файлами

Ранее построенный строгий decoder прочитал все 105 588 уникальных `spMaterialData`
объектов PC/PS2-корпуса до терминатора. Он наблюдает только поля
`0,1,2,3,4,6,8,9,10,11,12,17`, 11 render states, 9 texture states и от одного до трёх
pass. Подробная статистика и relationship variants находятся в
[`smo-class-sp-material-data.md`](smo-class-sp-material-data.md).

Это независимая проверка структуры, но не разрешение на запись: mutation/repack в игре
для материала ещё не проверялись.

## Проверка

Последовательная сборка `ninja -j1` и оба CTest-набора проходят. Тесты фиксируют RTTI,
portable standard write/index plans, legacy/current field IDs, отказ на неподдержанном
layer, clone и раздельные ABI layouts `0x3C` с helper по `+0x14`.

## Открытые вопросы

1. Original header и исходное имя embedded data-block helper type.
2. Прямой PC allocator/`sizeof` за protected factory entry и secondary vtable address.
3. Original enum names 11 render states, 9 texture states и `FinalBlendOp`.
4. Полные field IDs/layout для camera, movie, cube-map, render-target и UV-gen branches.
5. Точные status/rollback правила при частично созданном pass/layer.
6. Ownership/fixup concrete relationships и mutation/repack validation в игре.
