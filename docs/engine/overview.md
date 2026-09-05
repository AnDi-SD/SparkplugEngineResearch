# Обзор Sparkplug

## Граница исследования

Цель проекта — восстановить исходную архитектуру Sparkplug с оригинальными
именами типов и модулей, отделить от неё application layer Winx Club и затем
повторить точный механизм загрузки и использования ресурсов: от точки запроса в
executable через файл/PCK, serializer и runtime object до сцены и вывода. Это
должно позволить независимо читать, диагностировать, визуализировать и в
проверенных пределах изменять данные Winx Club: The Game. Полная декомпиляция
всей игровой логики для этого не требуется.

Главные источники evidence:

1. статический call graph и runtime trace конкретных PC/PS2 executable;
2. полные registrations: точные class names/hashes, inheritance и owners;
3. исходные module/source paths, platform-specific families и границы `sp/wx`;
4. байты чистых и изменённых ресурсов PC/PS2;
5. воспроизводимый вывод строгих parsers и corpus analyzers;
6. контролируемые изменения копий ресурсов с проверкой результата в игре;
7. сравнение прямых PC/PS2-пар и соседних ANM/SAN/SPT/SPL/STX-ресурсов.

## Подтверждённая слоистая модель

```text
FFPS container
  └─ object directory (name, class hash, logical offset, serialized size)
      └─ SBOO object + spDataBlockSerializer fields
          └─ Sparkplug object graph
              ├─ model / mesh / render node / skin
              ├─ material / controller / texture data
              ├─ node / partition / zone / portal / navigation
              ├─ collision / bounds / occlusion
              └─ fog / light / particles / lens / sky / text
```

SMO нельзя надёжно читать как поиск одной vertex/index-сигнатуры. Сначала нужен
контейнер и каталог, затем точные serializer-секции и relationships, и только
потом интерпретация геометрии, материалов, текстур и поведения объекта.

## Что подтверждено

- Проиндексированы отдельные `pc-pristine`, `pc-working` и `ps2-pristine`:
  1 149 SMO-копий, 36 наблюдаемых class ID на PC, 32 на PS2 и ни одной ошибки
  структурного разбора.
- Та же schema v5 индексирует все 14 490 уникальных версий файлов: Media/PCK,
  корни установок, PC PE, PS2 ELF, конфиги, звук, видео, локализацию и известные
  зависимости. Неподтверждённые форматы и неоднозначные ссылки остаются явно
  помеченными, а не угадываются.
- Все 36 встреченных классов имеют строгий structural/read-only decoder,
  corpus variants и evidence. Реестр содержит 50 классов; ещё один находится в
  SAN, а 13 выбранных executable-классов не встречены в SMO.
- Контейнер little-endian начинается с `FFPS`; исполняемые файлы обеих платформ
  проверяют также слово `0x26`.
- Каталог хранит имя, class hash, logical offset и serialized size. Физический
  адрес равен `DataStart + logicalOffset`; интервалы могут быть вложенными.
- Полностью разобраны наблюдаемые object/render graph relationships, 13 PC D3D
  vertex layouts, native PS2 mesh boundaries, PC skin weights/palettes,
  collision geometry и navigation topology.
- Primitive type `2` — triangle list. Прежние «PS2 preamble/boundary» являются
  нормальными native PS2 mesh внутри пяти PC `Menus/*_ps2.smo`.
- Наблюдаемые material/layer/texture relationships разрешаются через object
  graph. Fixed-size замена BGRA RGB подтверждена игрой; произвольный texture
  repack совместимым с runtime не считается.
- PC SAN содержит воспроизводимую шкалу времени; Vector3 интерполируется линейно,
  quaternion — slerp. Связь имеет вид `EXE -> ANM -> SAN -> track name -> SMO
  node name`, а не сырой pointer. Нативная one-byte mutation доказала, что PC
  сравнивает имя точно и регистрозависимо: case-only rename сохраняет валидность
  SMO, но создаёт другой ключ и разрывает привязку. Missing parent/leaf и
  duplicate names в обоих порядках также не отвергают модель; duplicate
  namespace сворачивается в один exact key.
- CollisionInfo, MeshBV, `wxFaceData`, surface types, BSP/octree/zone/portal,
  occlusion и navigation layouts известны для всего текущего корпуса.
- В PC и PS2 executable полностью восстановлены class registrations и
  непосредственное inheritance; PS2 ELF содержит баннер `Sparkplug Engine v1.0`,
  PC executable — пути serializer `.cpp` и идентификатор PDB.
- Полный PC registration graph содержит 733 типа: 329 engine `sp...` и 404 game
  `wx...`; 42 игровых типа прямо наследуют Sparkplug-типы. Пути исходников
  подтверждают модули `SparkBase`, `SparkBasePC`, `Sparkplug`, `SparkplugDX` и
  `SparkplugPC`. PS2 graph содержит 681 тип: 275 engine, 406 game и 44 прямых
  ребра `wx -> sp`. С PC совпадают 231 engine и 399 game types, без расхождений
  class hash или base class hash.
- PC `WinxClub.exe` получает корень ресурсов из
  `HKLM\\Software\\Konami\\Winx Club\\MediaPath` и строит пути через таблицы
  каталогов.
- Нативное PC-меню Resolution показывает три 4:3 режима, код применения понимает
  четвёртый индекс `1600x1200`; перспективная камера использует FOV 60 градусов,
  GUI camera — виртуальные размеры `100x75`.

Детали приведены в [описании SMO](../formats/smo.md),
[реестре типов](../reference/smo-object-types.md),
[описании физики и коллизий](physics-and-collision.md),
[платформенном сравнении](../platforms/pc-vs-ps2.md) и
[базе корпусов](../research/smo-corpus-database.md) и
[общей базе ресурсов](../research/game-resource-database.md) и
[карте оригинальной архитектуры](original-architecture.md) и
[карте runtime-конвейера](runtime-resource-pipeline.md).

## Что остаётся неизвестным

Неизвестных class ID и необъяснённых границ наблюдаемых payload больше нет.
Открытые вопросы относятся к четырём разным категориям:

- точный producer/generator FFPS export/session tag в `0x08`; serializer version
  `0x04=0x26` и platform mask `0x10` уже подтверждены;
- loader lifecycle: владение inline-объектами, разрешение ID и скрытые
  service/target bindings, из-за которых catalog-safe repack может падать;
- runtime-семантика отдельных полей: material states/blend, collision flags,
  navigation alternatives, effects, GUI anchors и missing/duplicate animation
  name behavior;
- ненаблюдавшиеся serializer-ветви и platform-native данные: PS2 DMA/VIF,
  texture swizzle, optional BSP/BV/Light/Text/Texture fields.

Канонический перечень находится в
[открытых вопросах](../../research/open-questions.md). Быстрые игровые проверки,
которые не требуют разработки универсального writer, собраны в
[runtime-плане](../research/smo-runtime-validation-plan.md).

## Соседние форматы

| Формат | Наблюдаемая роль | Текущий статус |
|---|---|---|
| `SMO` | объектный ресурс: graph, geometry, material, texture, scene и вспомогательные классы | 36/36 наблюдаемых классов структурно разобраны; runtime/writer вопросы открыты |
| `SAN` | FFPS-анимация с именованными tracks | PC layout, timing и interpolation разобраны; PS2 playback не завершён |
| `ANM` | таблица состояний и ссылок на SAN | последняя колонка/SAN и весь disk corpus проверены; семантика первых семи колонок открыта |
| `SPT` | игровые компоненты и ссылки на ресурсы | частично исследован, полный dependency/runtime contract не закрыт |
| `SPL` | размещение экземпляров/шаблонов уровня | частично исследован, полный placement contract не закрыт |

Просмотр отдельного SMO уже опирается на подтверждённый object graph. Полноценная
игровая сцена дополнительно требует SPT/SPL, runtime-created GUI/gameplay objects
и выбранных ANM/SAN поверх общего serializer.

## Следующий этап evidence

Следующий этап — не новый последовательный разбор классов и не серия визуальных
mutation-тестов сама по себе. Сначала восстанавливается непрерывный executable
path `request -> stream -> FAT -> serializer -> runtime object -> scene ->
draw`, начиная с `Alfea02.smo`. Corpus и length-preserving mutations затем
используются как контрольные входы для уже найденных consumers. PC и PS2 reverse
ведутся вместе: PC даёт runtime trace и D3D endpoint, PS2 — независимый и менее
защищённый статический вариант тех же engine-функций.
