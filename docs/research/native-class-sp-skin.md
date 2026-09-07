# Нативные `spSkin` и `spSkinSerializer`

Дата обновления: 2026-09-07, CP69. Статус: существенный PC-срез; PS2 сознательно
отложен, потому что для текущих выводов PC executable оказался достаточен.

Последние подтверждения: [фабрики и stream graph CP62](native-pc-skin-serialization.md),
[настоящая clone transaction CP63](native-pc-skin-clone.md),
[полный unlit Skin render CP64](native-pc-skin-render.md). Copy при отсутствии
mapped кости создаёт новую Node; повторные ссылки зависят от root depth.
Source выполняет palette -> mesh/material -> cached shader constants -> draw
для явно ограниченной fallback/NULL-fog ветви.

[CP65](native-pc-skin-loaded-render.md) связал настоящее чтение с render на
том же графе; [CP66](native-pc-skin-san-render.md) добавил настоящий SAN
reader/actor tick между чтением кости и Skin palette. Четыре real-SAN момента
совпали побитово до shader constants/device draw. Межфазная очистка runtime
явная; одновременно живой полный кадр ещё не заявляется.

[CP69](native-pc-skin-generated-render.md) исполнил тот же read→Skin render
при пустом shader cache: original template/compiler boundary/reflection/
device create/cache insert/constants/draw, включая отрицательный HRESULT.
Source передаёт optional generation context через существующие методы.

## Идентичность

`spSkin` имеет class ID `0x681F2043` и напрямую наследует `spModel`
(`0x763277DB`). `spSkinSerializer` имеет ID `0x120D33C7`, напрямую наследует
`spModelSerializer` (`0xDB55C34A`) и обслуживает именно `spSkin`.

| Факт | `spSkin` | `spSkinSerializer` |
|---|---:|---:|
| registration | `0x00760520` | `0x00762950` |
| initializer | `0x006D3B20` | `0x006D4880` |
| protected factory entry | `0x0046A120` | `0x00490C50` |
| registration getter | `0x0046A0F0` | `0x00490C10` |
| destructor | `0x0046A1F0` | `0x00490C20` |
| deleting destructor | `0x0046A7A0` | `0x00490D10` |
| clone | `0x0046A1A0` | `0x00490CC0` |
| primary vtable | `0x006E8C5C` | `0x006EC7CC` |
| secondary stream vtable | — | `0x006EC7C0` |
| observed extent | `0x70` | `0x14` |

CP62 полностью исполнил обе защищённые фабрики: точные allocation sizes
`0x70` и `0x14`; Skin по умолчанию имеет `+60=4` и пустые массивы.
У serializer собственных полей после базового `0x14` не обнаружено.
Для него также восстановлен точный путь translation unit:
`Z:\Sparkplug\Code\Sparkplug\spSkinSerializer.cpp`.

## Runtime-layout `spSkin`

После PC-layout `spModel` размером `0x60` расположены четыре слова:

| Offset | Тип | Назначение |
|---:|---|---|
| `+0x60` | `uint32` | weight/influence count, первое слово `esfSkin` |
| `+0x64` | `uint32` | число костей/palette slots |
| `+0x68` | `spNode**` | массив отношений к костям |
| `+0x6C` | `float (*)[16]` | параллельный массив inverse-bind матриц |

Тело `0x0046A100` атомарно присваивает count и оба указателя. Destructor
`0x0046A1F0` освобождает массивы `+0x68` и `+0x6C`, затем передаёт разрушение
в `spModel`; сами заимствованные Node он не освобождает. Copy body
`0x0046A650` сначала освобождает прежние массивы destination, затем копирует base, переносит два
счётчика, выделяет `boneCount * 4` и `boneCount * 64` байт, копирует матрицы и
для каждого bone relationship запрашивает соответствие у clone manager.

Эти поля совпадают с уже доказанным wire payload `esfSkin`: два `UInt32`, затем
для каждого слота relationship к `spNode` и матрица `float32[16]`. Название
первого слова как точного original member пока неизвестно; corpus-карточка
использует аналитическое имя `BlendInfluenceCountHint`.

## Выход в renderer

`spSkin::0x0046A240` выполняет inherited pre-render, проходит все кости,
комбинирует transform каждой `spNode` с соответствующей inverse-bind matrix и
заполняет renderer bone palette. Затем функция публикует `boneCount` в поле
renderer `+0xC9BC`, передаёт base mesh через уже подтверждённый renderer slot
`9`, выполняет post-render и очищает временный bone count **только на успешной
ветви** `0x0046A38B`. Error exits через `0x0046A382` этот сброс не выполняют.

Это замыкает PC-цепочку:

```text
SMO esfSkin -> spSkin arrays -> spNode transforms + inverse bind
             -> renderer bone palette -> spModel base mesh submission
```

Порядок умножения закреплён в часовом PC-проходе 2026-09-05:
`palette = inverseBind * boneWorld` в row-indexed storage. Call site
`0x0046A2AA` передаёт inverse-bind как `this` в `0x00426B00`; некоммутирующий
тест translation/scale и 64 matrix pairs воспроизведены оригинальными
инструкциями вместе с native copy `0x0041D330`. Portable helper
`ComposePaletteMatrixForAnalysis` добавлен в `spSkin`.
Builder `0x00461D70` получает cached node position/scale/orientation
`+0x74/+0x80/+0x8C`; его entry защищён, world-cache update ещё открыт.
Подробнее: [PC animation runtime](native-pc-animation-runtime.md).

## Serializer

Три главных entry point восстановлены без обращения к PS2:

- `0x00490D30` сначала индексирует `spModel`, затем все bone relationships;
- `0x00490DA0` записывает base-секцию, всегда открывает field `0` (`esfSkin`),
  пишет два счётчика и для каждого слота relationship плюс ровно 64 байта
  матрицы, после чего завершает field;
- `0x00491170` сначала читает base, принимает только field `0`, выделяет оба
  массива и читает отношения класса `spNode` (`0x695C0F65`) и матрицы. Null
  bone считается ошибкой и ведёт к нативному диагностическому пути.

Это подтверждает, что найденная corpus-грамматика не была эвристикой decoder-а:
writer и reader executable реализуют ту же последовательность. CP62 добавил
portable read/index/write adapters поверх общего resolver/FAT; десять
сценариев сверяются с полными оригинальными Skin/Model/Node функциями.
Подробности и явно отличающееся безопасное владение:
[Skin serialization CP62](native-pc-skin-serialization.md).

## Реконструкция и проверка

Добавлены `Sparkplug/Code/Sparkplug/spSkin.*` и
`spSkinSerializer.*`, PC ABI structs/constants и CTest. Portable `spSkin`
владеет безопасным параллельным набором bone/matrix, отклоняет null bones,
сохраняет base state при clone и повторяет известный clone-manager remap там,
где окружающий граф уже зарегистрировал копию узла.

Read-only проверка привязана к SHA-256 pristine PC executable и фиксирует
registration bodies, обе vtable, offsets, copy/render paths, reader/writer и
исходный путь serializer:

```powershell
python research\inspect_skin.py
```

Результат skin inspector: `PASS`, 21/21 проверок. Полная portable-сборка проходит;
после часового runtime-цикла CTest содержит три успешных набора:
`SparkBaseTests`, `SparkplugEngineTests`, `SparkplugAnimationTests`.

## Что ещё неизвестно

1. Arbitrary cyclic clone graphs и ошибки копирования ещё открыты;
   root/direct transactions, repeated bones и child mapping подтверждены CP63.
2. Не закреплены original member/method names и full-frame scheduling;
   node world-cache transform и полный affine builder теперь проверены,
   конвенция умножения palette уже доказана.
3. Полный SMO loader/exporter с Skin/mesh/material и все ошибки I/O не закрыты;
   scalar/graph section adapters, общие ссылки и индексирование доказаны CP62.
4. Передача весового слова 0/FFFFFFFF через reader/writer подтверждена;
   CP64 подтверждает, что Skin render не использует это слово, а получает
   shader key от mesh vertex components. Остальная runtime-мутация открыта.
5. PS2 implementation остаётся `deferred`: к ней следует обращаться только
   если PC-анализ оставит вопрос, влияющий на общий контракт или exporter.

Track-to-local-node путь теперь подтверждён через `spActor`,
`spTransformTrackEval` и `spNodeController`. Dirty propagation и cached world
PRS/builder закрыты последующим [PC node-проходом](native-pc-node-world.md).
Открыты outer frame caller и связанная renderer integration.
