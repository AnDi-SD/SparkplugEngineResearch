# `spNode` (`0x695C0F65`)

Статус: общий PC/PS2 serializer полностью восстановлен для чтения. Все девять
полей подтверждены кодом обоих executable, а все наблюдаемые значения и связи
проверены по трём корпусам. Viewer показывает поля и использует обе формы
`esfNodeChild` при построении логического графа. Запись намеренно не включена до
native-проверки изменённого графа.

## Распространённость

| Корпус | Уникальные объекты | SMO | Физические вхождения объектов | Размер объекта |
|---|---:|---:|---:|---:|
| `pc-working` | 17 841 | 416 | 17 841 | 11–10 792 947 |
| `pc-pristine` | 17 841 | 416 | 17 841 | 11–10 792 947 |
| `ps2-pristine` | 15 714 | 317 | 54 176 | 11–7 057 420 |

Все 51 396 уникальных корпусных строк имеют имя. `spNode` присутствует в каждом
SMO. Миллионные размеры не являются отдельными blob-вариантами узла: field 5
может inline содержать полное сериализованное поддерево.

## Поля serializer

| Field | Имя executable | Payload | Default / правило записи | PC working | PC pristine | PS2 |
|---:|---|---|---|---:|---:|---:|
| 0 | `esfNodePosition` | `Vector3` | `(0,0,0)`, default опускается | 15 444 | 15 444 | 13 841 |
| 1 | `esfNodeRotation` | quaternion XYZW | identity, default опускается | 9 186 | 9 186 | 8 166 |
| 2 | `esfNodeScale` | `Vector3` | `(1,1,1)`, default опускается | 173 | 173 | 125 |
| 3 | `esfNodeIsBone` | byte boolean | `false`, записывается только `true` | 5 197 | 5 197 | 4 699 |
| 4 | `esfNodeIsStatic` | byte boolean | `false`, записывается только `true` | 360 | 360 | 47 |
| 5 | `esfNodeChild` | object relationship | повторяемое поле | 31 921 | 31 921 | 26 084 |
| 6 | `esfNodeBillboardAxis` | `UInt32` enum | `0`; writer поддерживает `1/2` | 0 | 0 | 0 |
| 7 | `esfNodeCollision` | object relationship | повторяемое поле | 151 | 151 | 130 |
| 8 | `esfNodeIsAnimated` | byte boolean | текущий writer пишет явно | 17 472 | 17 472 | 15 714 |

Field 6 подтверждён обоими executable, но у объектов точного типа `spNode` не
встретился. Названия двух осей пока не восстановлены, поэтому Viewer честно
показывает `engine axis 1/2`, а не подставляет догадку X/Y.

Field 8 имеет legacy-исключение. В каждом PC-корпусе 16 544 значения равны
`false`, 928 — `true`, а 369 старых объектов не содержат поле; read-only decoder
использует для них форматный fallback `false`. Это не constructor-default
текущего runtime: PS2 constructor `0x001A8EE0` явно пишет flags `0x00070A00`,
включая animated bit `0x800`. Точное поведение native reader для отсутствующего
legacy-поля остаётся открытым. PS2 явно хранит 14 887 `false` и 827 `true` во
всех 15 714 объектах. Текущие PC и PS2 writer-функции поле не опускают.

## Child и collision relationships

Обычная форма relationship:

```text
UInt32 objectId
UInt32 inlineSerializedSize
byte   inlineSboo[inlineSerializedSize]
```

`inlineSerializedSize == 0` означает ссылку на уже сериализованный объект. При
ненулевом размере inline-данные начинаются с class ID и `SBOO`; размер точно
совпадает с `SerializedSize` соответствующей строки object directory.

Найдена также компактная PC-форма field 5 из одного `UInt32 objectId`. Она
встречается в `Characters/Knut/staff_projectile.smo` (две связи Scene Root) и
`Levels/Gardenia/test_world_winx.smo` (пять связей Scene Root), одинаково в
working и pristine PC. Все 14 корпусных поля разрешаются в существующие объекты.
PS2 эту форму не использует.

| Корпус | ID-only, 4 байта | Sized reference, 8 байт | Inline object |
|---|---:|---:|---:|
| `pc-working` | 7 | 1 445 | 30 469 |
| `pc-pristine` | 7 | 1 445 | 30 469 |
| `ps2-pristine` | 0 | 1 584 | 24 500 |

Все 432 field 7 имеют inline-форму и разрешаются в `spCollisionInfo`. Field 5
ссылается на классы, являющиеся node-ролевыми объектами движка:

| Target class | Связи в трёх корпусах |
|---|---:|
| `spNode` | 50 095 |
| `spRenderNode` | 37 973 |
| `spLightData` | 1 482 |
| `spSkyBox` | 126 |
| `spPartitionSystem` | 88 |
| `spNavigationGraph` | 80 |
| `spOcclusionVolume` | 60 |
| `spTextNode` | 20 |
| `spZone` | 2 |

Object-directory `ParentIndex` по-прежнему означает физическое владение
serializer-интервалом. Логический parent/child граф строится только по field 5;
теперь в него входят inline, sized-reference и ID-only формы.

## Свидетельства executable

Pristine PC `WinxClub.exe`:

- class registration связывает `spNode` с hash `0x695C0F65`, а
  `spNodeSerializer` — с `0x4545848A`;
- serializer `0x00463F10..0x00464719` использует ID `0,1,2,3,4,8,5,6,7`;
- position сравнивается с нулём, rotation с identity, scale с единицей;
- флаги bone/static берутся из bit `12/10` и пишутся только для `true`, animated
  берётся из bit 11 и передаётся serializer всегда;
- field 5 проходит список детей, field 7 — vector collision-info; field 6
  кодирует runtime flags bit 20/21 значениями `1/2`.

PS2 `SLES_532.19` независимо подтверждает тот же контракт:

- serializer `0x00196CD0..0x001974B0` вызывает те же typed writer-функции с теми
  же ID, defaults и циклами relationships;
- xrefs строк полей находятся около `0x00196F1C..0x00197450`;
- функция `0x001974C0` возвращает class hash `0x695C0F65`.

Runtime-layout, lifetime и portable reconstruction отдельно зафиксированы в
[`native-class-sp-node.md`](native-class-sp-node.md).

PC runtime дополнительно подтвердил слой привязки имени к transform evaluator.
Class registration связывает ID `0x5DAF152D` с `spTransformTrackEval`, а запись
по `0x00454628` устанавливает его поле `+0x10` после exact name lookup:

- pristine: `L_Toe -> 0x3B`, `R_Toe -> 0x3F`, `foot_right -> 0x46`; до binding
  поле evaluator равно `0xFFFFFFFF`;
- same-length missing mutations `Z_Ankle` и `Z_Toe` получают новый отдельный
  slot `0xD8`, не прежний slot исходного имени; descendant `foot_right` при этом
  остаётся точным ключом со slot `0x46`;
- в обоих duplicate-порядках два разных evaluator получают один slot оставшегося
  имени (`0x3B` или `0x3F`). Это подтверждает all-target binding и исключает
  first-only/last-only на этом слое.

Virtual method `0x005FEBB0` распознан как transform-track evaluator и включён в
probe для PRS, однако contextual route `startLevel=2` только загружает и связывает
Bloom: в трёх контрольных запусках tracked evaluator не вошёл в активный tick.
Поэтому binding slots подтверждены, а final local/world matrices остаются
отдельным runtime-вопросом.

Дополнение часового PC-цикла 2026-09-05: отдельные native instruction probes
подтвердили evaluator → controller → local PRS, cached-world offsets и
матричную конвенцию; найден visible tail world updater `0x00421420`.
Это не меняет отрицательный результат старых игровых `startLevel=2` запусков:
полная frame integration ещё не проверена. Адреса, source slices и ограничения
см. в [PC animation runtime](native-pc-animation-runtime.md).

## Сопоставление корпусов

Сравнение нормализует object ID, inline-размер и платформенное тело вложенного
объекта. В signature остаются имя узла, эффективные transform/flags и
последовательность target class/name отношений.

- working/pristine PC: совпадают 387/416 ресурсов. Отличающиеся 29 ресурсов
  действительно имеют изменённое содержимое графа, прежде всего рабочие level
  assets; layout у них тот же;
- pristine PC/PS2: совпадают 288/317 общих ресурсов. Оставшиеся 29 содержат
  платформенно отличающийся граф, но используют те же поля и encodings;
- это один serializer-вариант, а bone/static/animated/children/collision —
  независимые свойства, не подтипы класса.

## Viewer и база

`SmoNodeDecoder` применяет подтверждённые defaults, декодирует quaternion,
флаги, billboard enum и обе relationship-формы. Read-only inspector показывает
каждое поле. `SmoNodeHierarchy` больше не теряет четыре-байтовые PC-ссылки.

Команда

```text
SmoViewer.Inspect research-db analyze-class <db> spNode
```

проверяет 51 396 объектов, 228 614 полей, 89 926 child-отношений и 432
collision-отношения, executable-токены и межкорпусные signatures. Она
идемпотентно записывает:

- девять common PC/PS2 field definitions, включая ненаблюдаемый field 6;
- decoded JSON всех наблюдаемых полей и target metadata relationships;
- вариант `node_serializer_contract` и 51 396 назначений;
- четыре evidence-записи: PC, PS2, полный корпус и cross-corpus.

Безопасная мутация остаётся открытой: изменение transform/flags или состава
отношений нужно отдельно проверить в native runtime с пересборкой inline sizes и
object directory.
