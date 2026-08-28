# Обзор Sparkplug

## Граница исследования

Цель проекта — документировать загрузку ресурсов Sparkplug настолько, чтобы
независимо читать, диагностировать, визуализировать и в проверенных пределах
изменять данные Winx Club: The Game. Это не попытка полностью заново реализовать
движок или всю игровую логику.

Главные источники evidence:

1. байты чистых и изменённых ресурсов PC/PS2;
2. воспроизводимый вывод строгих parsers и corpus analyzers;
3. строки регистрации классов, serializers и call sites в исполняемых файлах;
4. контролируемые изменения копий ресурсов с проверкой результата в игре;
5. сравнение прямых PC/PS2-пар и соседних ANM/SAN/SPT/SPL/STX-ресурсов.

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
- Все 36 встреченных классов имеют строгий structural/read-only decoder,
  corpus variants и evidence. Реестр содержит 49 классов; 13 не встречены в SMO.
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
- В PC и PS2 executable найдены class registrations и serializer tokens; PS2 ELF
  содержит баннер `Sparkplug Engine v1.0`, PC executable — пути serializer `.cpp`
  и идентификатор PDB.
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
[базе корпусов](../research/smo-corpus-database.md).

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

Следующий этап — не новый последовательный разбор классов, а runtime-валидация
сразу по всем системам: baseline sweep debug menu, загрузка уровней и персонажей,
length-preserving изменения заголовка, имён, transforms, materials, collision,
navigation, effects и GUI. Долгий PS2 binary reverse и универсальный structural
writer выполняются позже вместе с развитием софта.
