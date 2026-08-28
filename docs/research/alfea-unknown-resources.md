# Неизвестные классы Алфеи и привязка анимаций

Дата проверки: 2026-08-28. Корпус — девять PC SMO из
`Media/Levels/Alfea`: обычная, broken и night версии частей 01–03. Все файлы
разобраны строгим parser без ошибок. Имена классов сопоставлены по паре
`hash immediate + строка регистрации класса` в `WinxClub.exe`; структуры полей
дополнительно сверены со строками ошибок serializer и экземплярами в SMO.

## Результат по неизвестным class ID

| Class ID | Класс | Объектов | Файлов | Наблюдение |
|---:|---|---:|---:|---|
| `0x5E6402DF` | `spLightData` | 133 | 9 | источники света |
| `0x7362AB22` | `spBSPNode` | 55 | 9 | узлы BSP |
| `0x7AC95AEC` | `spFog` | 9 | 9 | один fog-объект на SMO |
| `0x5AFA1A4F` | `spParticleSystem` | 14 | 2 | fire/point emitters |
| `0x385662AA` | `spNavigationPortal` | 10 | 1 | `NavPortal: nv...` |
| `0x7297173C` | `spMeshNavigationSet` | 7 | 1 | navigation sets |
| `0x188A161F` | `spNavigationGraph` | 1 | 1 | `Navigation Graph` |

После добавления этих пар неизвестных class ID в девяти SMO Алфеи не осталось.
Последующий полный corpus-разбор закрыл и payload этих классов:
`spMeshNavigationSet`, `spNavigationPortal` и `spNavigationGraph` теперь строго
декодируются на PC/PS2, включая routing tables и все alternative paths.

## Подтверждённые поля собственной секции

Sparkplug сериализует секции наследования подряд и завершает каждую пустым
`field 0`. Номера полей между секциями повторяются. Поэтому имя ниже относится
только к последней, собственной секции указанного класса.

- `spFog`: `f0` — type, ARGB color, start, end, density. Позднейший полный
  PC/PS2-разбор подтвердил один 20-байтовый layout и два наблюдаемых type-варианта;
  см. [`smo-class-sp-fog.md`](smo-class-sp-fog.md).
- `spLightData`: `f0` type; `f1` project shadow; `f2` ARGB color; `f3`
  attenuation; `f4` intensity; `f5` range; `f6` hotspot; `f7` falloff; `f8`
  enabled. Полный PC/PS2-разбор, defaults и типы света вынесены в
  [`smo-class-sp-light-data.md`](smo-class-sp-light-data.md).
- `spNavigationGraph`: `f0` navigation set relation; `f1` portal relation;
  `f2` path-table size; `f3` полная square row-major path table. Точный layout,
  next-hop и alternatives: [`smo-class-sp-navigation-graph.md`](smo-class-sp-navigation-graph.md).
- `spMeshNavigationSet`: собственный `f0` — mesh relation. Базовый
  `spNavigationSet` хранит `NodeCount`, node/portal routing matrices, ordered
  adjacency, portals и `Enabled`. Все поля и terminal marker 3 подтверждены на
  PC/PS2, но согласованное изменение graph пока отключено. Полный разбор:
  [`smo-class-sp-mesh-navigation-set.md`](smo-class-sp-mesh-navigation-set.md).
- `spNavigationPortal`: `f0` graph relation; `f1` две set relation; `f2` пары
  navigation nodes; `f3` source/destination/alternative membership. Полный
  разбор: [`smo-class-sp-navigation-portal.md`](smo-class-sp-navigation-portal.md).
- `spBSPNode` имеет сначала унаследованную секцию `spPartitionNode` с двумя
  branch slots 0/1, затем собственный `f0` Plane (normal + constant).
  Собственный optional `f1` Polygon (count + vertices) поддержан обоими
  executable, но отсутствует во всех PC/PS2-объектах корпуса. Полный разбор:
  [`smo-class-sp-bsp-node.md`](smo-class-sp-bsp-node.md).
- `spParticleSystem`: `f0..f6` диапазоны acceleration/direction/velocity/
  angle/scale/color/time; `f7..f9` loop/world-space/iterative; `f10` rate;
  `f11` bounding radius; `f12..f18` emission regions; `f19` render-node
  relation. Все семь region layouts подтверждены на полном PC/PS2-корпусе:
  [`smo-class-sp-particle-system.md`](smo-class-sp-particle-system.md).

`SmoViewer` показывает эти сведения и составные значения только для чтения.
Полный layout теперь известен; для безопасного редактора всё ещё нужны writers,
атомарное обновление связанных объектов и runtime-тесты каждого изменения.

## Как на самом деле связываются герой и анимация

Наблюдаемая цепочка состоит из трёх отдельных соединений по строкам:

```text
WinxClub.exe: каталог имён *.anm
                  ↓ имя файла
*.anm: таблица состояний, последний столбец = *.san
                  ↓ имя файла
*.san: spAnimation, у каждого track есть NodeName
                  ↓ совпадение имени
*.smo: именованный spNode или spRenderNode
```

Это не сырой указатель из EXE в SMO. Поэтому замена имени кости/узла в SMO не
делает контейнер невалидным и обычно не обязана приводить к падению: SAN
загружается, но соответствующий track больше не находит цель. Результат — bind
pose, частичная или визуально сломанная анимация. Если в ANM указать другой
существующий SAN, свяжется только пересечение имён его tracks и узлов модели.

Статический аудит дал следующие результаты:

- на диске 73 ANM, 2782 строки и 519 уникальных ссылок на SAN; отсутствующих
  SAN среди ссылок нет;
- в EXE найдено 72 уникальных имени ANM, и все они существуют на диске;
  дополнительный `AdvBloom.anm` не входит в найденный блок EXE;
- три Alfea-таблицы Bloom содержат 118 строк и ссылаются на 59 уникальных SAN;
- для этих 59 SAN и `bloom_jeans.smo`: 4192 tracks, 3832 точных совпадения,
  360 tracks без цели и ни одного совпадения только после смены регистра;
- 360 непривязанных tracks сводятся к 16 служебным/DCC-именам, включая
  `Master`, `Ct_ladder`, `Bloom_hairs`, `WingL`, `WingR`, `Omni01` и helper
  objects. Их наличие допустимо: один SAN может обслуживать несколько вариантов
  модели или содержать экспортированные служебные tracks.

Регистрозависимость дополнительно проверена нативным PC runtime на отдельной
копии `bloom_jeans.smo`: имя object `[14]` изменено same-length заменой
`R_Ankle -> r_Ankle` ровно в одном байте по `0x1B0`, ANM/SAN/STX оставлены
pristine. Обе модели прошли нативную загрузку, но точный comparator дерева имён
дал разные результаты:

- pristine после загрузки модели выполнил 32 сравнения ключа `R_Ankle`, включая
  два точных `R_Ankle == R_Ankle`;
- mutation выполнила 16 сравнений отдельного ключа `r_Ankle`, не получила ни
  одного равенства и ни разу не сравнила его с `R_Ankle`; последующие независимые
  запросы `R_Ankle` продолжили находить штатный ключ;
- все 41 статические ссылки на `_stricmp`, две на `_strcmpi` и одна на
  `lstrcmpiA` были покрыты probes и не участвовали в этой привязке;
- обработавший имя код `0x013BBCC4..0x013BBD13` является точным
  `std::map`-подобным лексикографическим поиском через
  `char_traits<char>::compare`.

Следовательно, PC lookup имени node/track регистрозависим: несовпадение регистра
не повреждает контейнер и не обязано ронять игру, но создаёт другой ключ и ломает
same-name relation. Диагностический анализатор по-прежнему отдельно показывает
exact, case-only и missing, где case-only означает проблему, а не допустимый bind.

## Оставшиеся runtime-вопросы привязки

- Применяется ли track к first, last или всем SMO nodes с одинаковым именем.
  Loader принимает оба порядка, а строковый namespace сворачивает их в один key;
  error/rejection и две независимые name-записи уже исключены.
- Какой визуальный fallback используется для отсутствующего target и
  распространяется ли bind pose на descendants. Loader уже подтверждённо
  допускает missing parent и leaf без отказа или crash.
- Являются ли 16 service/DCC-имён намеренными tracks для других вариантов
  модели либо обрабатываются отдельной runtime-системой.
- Как выбирается ANM-таблица для героя/состояния и почему `AdvBloom.anm` не входит
  в найденный EXE-блок.
- Что означают первые семь колонок ANM; последняя SAN-ссылка уже подтверждена.

Проверки выполняются same-length переименованиями на отдельной копии персонажа,
а не случайной заменой resource pointers. Порядок экспериментов:
[`smo-runtime-validation-plan.md`](smo-runtime-validation-plan.md).

## Инструментальная проверка

Для воспроизводимого анализа добавлена команда:

```text
SmoViewer.Inspect animation-bindings <model.smo> <file.san|directory> [--json]
```

Она декодирует SAN, сравнивает имена tracks только с `spNode`/`spRenderNode` в
SMO и сообщает точные, различающиеся только регистром, отсутствующие и
неоднозначные совпадения.
