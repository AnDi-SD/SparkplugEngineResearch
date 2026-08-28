# `spRenderNode` (`0x603625D0`)

Статус: общий двухсекционный PC/PS2 serializer полностью восстановлен для
чтения. Первая секция наследует все девять полей `spNode`, вторая содержит
повторяемое поле `esfRenderNodeRenderable`. Все наблюдаемые поля и отношения
проверены по трём корпусам и кодом обоих executable. Viewer показывает обе
секции, учитывает наследованные child-связи и применяет transform defaults.
Запись намеренно не включена до native-проверки пересобранных отношений.

## Распространённость

| Корпус | Уникальные объекты | SMO | Физические вхождения объектов | Размер объекта |
|---|---:|---:|---:|---:|
| `pc-working` | 14 064 | 413 | 14 064 | 12–1 087 199 |
| `pc-pristine` | 14 064 | 413 | 14 064 | 12–1 087 199 |
| `ps2-pristine` | 10 315 | 314 | 17 337 | 12–527 212 |

Все 38 443 уникальные корпусные строки имеют имя. Большой `SerializedSize`
объясняется inline renderable-поддеревьями, а не отдельным layout класса.

## Две serializer-секции

```text
UInt32 0x603625D0
char[4] "SBOO"

section 0: spNode fields 0..8
terminator

section 1: repeated field 0, esfRenderNodeRenderable
terminator
```

В исследовательской базе секции адресуются от конца объекта: наследованная
`spNode` имеет `section_from_end = 1`, собственная `spRenderNode` — `0`. Это
позволяет не путать одинаковый номер field 0 у position и renderable.

## Наследованные поля `spNode`

| Field | Семантика | Payload | PC working | PC pristine | PS2 |
|---:|---|---|---:|---:|---:|
| 0 | position | `Vector3` | 13 261 | 13 261 | 9 635 |
| 1 | rotation | quaternion XYZW | 8 512 | 8 512 | 6 066 |
| 2 | scale | `Vector3` | 4 917 | 4 917 | 3 610 |
| 3 | is bone | byte boolean | 0 | 0 | 0 |
| 4 | is static | byte boolean | 6 690 | 6 690 | 3 482 |
| 5 | child | object relationship | 210 | 210 | 196 |
| 6 | billboard axis | `UInt32` enum | 380 | 380 | 316 |
| 7 | collision | object relationship | 1 943 | 1 943 | 1 629 |
| 8 | is animated | byte boolean | 11 749 | 11 749 | 10 315 |

Defaults совпадают с `spNode`: position `(0,0,0)`, identity rotation, scale
`(1,1,1)`, false для флагов и `0` для billboard axis. Bone field в корпусе
`spRenderNode` не наблюдается. Static хранит только `true`. Billboard использует
значения `1` и `2`: суммарно 120 и 956 полей соответственно.

Animated имеет PC legacy-форму: в каждом PC-корпусе явно записаны 10 712
`false` и 1 037 `true`, ещё 2 315 объектов опускают default `false`. PS2 явно
хранит 9 341 `false` и 974 `true` во всех 10 315 объектах.

Наследованное поле child даёт 616 логических связей во всех трёх корпусах.
Field 7 даёт 5 515 связей с `spCollisionInfo`. Два различных объекта
`Levels/Gardenia/test_world_winx.smo` используют компактный четырёхбайтовый
collision reference; из-за двух PC-корпусов это четыре строки базы. Остальные
отношения имеют обычную sized/inline-форму.

## Собственное поле renderable

Field 0 собственной секции — повторяемое `esfRenderNodeRenderable`. Пустой
список допустим: он встречается у 1 907 объектов каждого PC-корпуса и у 1 678
PS2-объектов. Максимум — 27 renderable-связей на один узел.

| Encoding | PC working | PC pristine | PS2 | Всего |
|---|---:|---:|---:|---:|
| ID-only, 4 байта | 3 | 3 | 0 | 6 |
| Sized reference, 8 байт | 4 494 | 4 494 | 3 839 | 12 827 |
| Inline object | 14 857 | 14 857 | 10 510 | 40 224 |
| **Всего** | **19 354** | **19 354** | **14 349** | **53 057** |

Формы relationship совпадают с `spNode`:

```text
UInt32 objectId                                      // legacy ID-only
UInt32 objectId; UInt32 inlineSize                   // existing object reference
UInt32 objectId; UInt32 inlineSize; byte SBOO[...]   // inline object
```

При inline-форме `inlineSize` во всех случаях точно совпадает с
`SerializedSize` target-объекта. Все ID разрешаются в object directory, а inline
payload начинается с ожидаемых class ID и `SBOO`.

| Target class | Связи в трёх корпусах |
|---|---:|
| `spModel` | 49 529 |
| `spSkin` | 1 777 |
| `spParticleSystem` | 1 745 |
| `spLensFlare` | 6 |

Три различные компактные PC-связи найдены в
`Characters/Knut/staff_projectile.smo` (`projectile_staff`) и
`Levels/Gardenia/test_world_winx.smo` (`ramp`, `world_box`). Их шесть строк в
базе — это те же три поля в working и pristine PC, а не шесть разных layout.

## Свидетельства executable

Pristine PC `WinxClub.exe`:

- присутствуют `spRenderNodeSerializer.cpp` и `esfRenderNodeRenderable`;
- serializer `0x00469340..0x00469692` сначала вызывает `spNodeSerializer`, затем
  проходит список renderable и пишет собственный field 0;
- `push 7` в этом цикле является кодом расширенного размера заголовка, а не
  номером serializer field;
- class registration связывает объект с hash `0x603625D0`.

PS2 `SLES_532.19` независимо подтверждает контракт:

- serializer `0x001980A0..0x00198278` вызывает node serializer и затем проходит
  тот же field 0;
- функция около `0x00198280` возвращает class hash `0x603625D0`;
- строки класса, serializer source и поля совпадают с PC.

## Сопоставление корпусов

Signature нормализует object ID, inline-размер и платформенное тело target,
оставляя имя узла, эффективные transform/flags и последовательность классов и
имён всех отношений.

- working/pristine PC: совпадают 413/413 ресурсов;
- pristine PC/PS2: совпадают 190/314 общих ресурсов;
- 124 платформенно отличающихся графа используют тот же двухсекционный контракт;
  различается авторское содержимое, а не формат полей.

Renderable composition, billboard/static/animated flags и child/collision
отношения являются независимыми свойствами. Корпус не даёт оснований считать их
отдельными подвидами `spRenderNode`.

## Viewer и база

`SmoRenderNodeDecoder` возвращает декодированный наследованный `SmoNodeData` и
список renderable relationships. Реестр полей различает собственную и
наследованную секции; inspector показывает семантические имена и target metadata.
Hierarchy теперь включает child-поля `spRenderNode`, а transform resolver
применяет подтверждённые defaults даже при полностью пустой node-секции.

Команда

```text
SmoViewer.Inspect research-db analyze-class <db> spRenderNode
```

проверяет 38 443 объекта, 183 630 прямых полей и 53 057 renderable-связей. Она
идемпотентно записывает:

- десять field definitions: девять наследованных и одно собственное;
- decoded JSON всех наблюдаемых полей и metadata target-объектов;
- вариант `render_node_serializer_contract` и 38 443 назначения;
- четыре evidence-записи: PC executable, PS2 executable, полный корпус и
  cross-corpus comparison.

Безопасная мутация остаётся открытой. Для изменения списка отношений потребуется
пересчитать inline sizes, object directory и все охватывающие serialized ranges,
после чего проверить результат нативным loader игры.
