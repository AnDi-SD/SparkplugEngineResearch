# Нативные `spSkin` и `spSkinSerializer`

Дата проверки: 2026-09-05. Статус: существенный PC-срез; PS2 сознательно
отложен, потому что для текущих выводов PC executable оказался достаточен.

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

Factory обоих классов находится за защищённым thunk, поэтому `0x70` для
`spSkin` является полным наблюдаемым диапазоном полей, а не заявлением об
увиденном allocation size. У serializer собственных полей после базового
`0x14` не обнаружено. Для него также восстановлен точный путь translation unit:
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
в `spModel`. Copy body `0x0046A650` сначала копирует base, переносит два
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
`9`, выполняет post-render и очищает временный bone count.

Это замыкает PC-цепочку:

```text
SMO esfSkin -> spSkin arrays -> spNode transforms + inverse bind
             -> renderer bone palette -> spModel base mesh submission
```

Порядок умножения и original имя palette helper пока не названы: наличие обеих
матриц и renderer consumer доказано, но переносить математическую конвенцию в
portable API до отдельного теста нельзя.

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
writer и reader executable реализуют ту же последовательность. Portable
`spSkinSerializer` пока формирует проверяемый write plan и не притворяется
полным stream writer-ом: общий object-directory/fixup/rollback protocol ещё
должен быть перенесён в базовые serializer-классы.

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

Текущий результат: `PASS`, 21/21 проверок. Полная portable-сборка проходит,
оба CTest (`SparkBaseTests`, `SparkplugEngineTests`) успешны.

## Что ещё неизвестно

1. Тело PC-constructor/factory и точные значения полей по умолчанию защищены.
2. Не закреплены original member/method names и конвенция умножения palette.
3. Portable serializer ещё не выполняет реальную запись object directory и
   не воспроизводит полный error rollback reader-а.
4. Runtime-мутация первого слова `esfSkin` и его поведение при значении `0`
   требуют отдельного наблюдения.
5. PS2 implementation остаётся `deferred`: к ней следует обращаться только
   если PC-анализ оставит вопрос, влияющий на общий контракт или exporter.

Следующая прямая зависимость — `spAnimation` и точный путь
track-to-`spNode` binding/update, после чего skin palette можно связать с
фактическим animated transform, а не только с renderer submission.
