# `spAnimTexControllerSerializer`

Статус: восстановлены identity, RTTI/lifetime, раздельный PC/PS2 ABI, внешний
field, внутренний track payload и relationship pass. Portable-класс моделирует
только подтверждённый порядок и размеры фиксированных сегментов; переменный размер
inline relationship намеренно не выдаётся за четыре байта.

## Идентичность

Обе сборки регистрируют `spAnimTexControllerSerializer` с Class ID `0x77793754`,
прямым base `spSerializer` (`0x42429877`) и target `spAnimTexController`
(`0x16FB0E47`). Factory PS2 выделяет `0x14` байт; PC lifetime/vtable подтверждают
тот же observed extent без derived storage. Точный исходный path не найден.

## Wire grammar

Serializer всегда создаёт единственный внешний field 0
`esfAnimTexControllerBase` типа 7. Внутри записываются строго три последовательных
сегмента:

1. один `UInt32 frameCount`;
2. непрерывный массив `float time[frameCount]` размером `4 * frameCount`;
3. `frameCount` relationship-записей на texture.

Relationship не имеет постоянного wire-размера: ссылка может быть внешней либо
содержать inline-объект. Reader использует один `frameCount` для обеих параллельных
коллекций. Native target layout на обеих платформах:

| Offset | Содержимое |
|---:|---|
| `+0x3C` | указатель на массив времён |
| `+0x40` | указатель на массив texture relationships |
| `+0x44` | общий frame count |

Передаваемый relationship Class ID — `0x2F281E13`, то есть базовый `spTexture`.
Конкретный inline объект может быть `spTextureData`/platform leaf, что объясняет
старую corpus-формулировку, но не меняет native type gate. Отдельный virtual pass
обходит все элементы `+0x40` до `+0x44` и индексирует/разрешает каждую связь.

## PC evidence

Контрольный `WinxClub.exe` SHA-256:
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.

- registration initializer `0x006D2F10..0x006D2F35`, register call
  `0x006D2F30`, record `0x0075EB28`;
- registration getter `0x0043C080`, target hook `0x0043C0B0`;
- protected factory entry `0x0043C0C0`, destructor `0x0043C090`, clone
  `0x0043C130`, deleting destructor `0x0043C180`;
- relationship pass `0x0043C1A0`, inner writer `0x0043C200`, inner reader
  `0x0043C2C0`, outer reader `0x0043C470`, outer writer `0x0043C5D0`;
- primary/interface vtables `0x006DFE20` / `0x006DFE14`;
- class string `0x006E0168`.

## PS2 evidence

Контрольный `SLES_532.19` SHA-256:
`198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE`.

- registration initializer `0x00483110`, record `0x004A9F10`;
- registration getter `0x00185030`, target hook `0x001855C0`;
- reader `0x00185040`, relationship pass `0x001852B0`, writer
  `0x00185360`;
- deleting destructor `0x001855D0`, clone `0x00185640`, factory
  `0x00185720`, exact allocation `0x14`;
- read/fixup/write thunks `0x00185790`, `0x001857A0`, `0x001857B0`;
- primary/interface vtable headers `0x0048F0B0` / `0x0048F0D4`;
- class string `0x0044AA50`.

## Сверка с корпусом

Read-only декодер ранее нашёл 28 controller-объектов и 974 frame-записи. Их
структура `count -> all times -> all relationships` совпадает с обеими native
реализациями. Статистика остаётся свойством исследованного корпуса, а не пределом
движка.

## Неизвестное

Дополнение PC, checkpoint20 (6 сентября 2026): актуальные наблюдения о
factory/lifetime, владении frame slots, interval-end выборе и strict-greater
wrap записаны в [material controllers](native-pc-material-controllers.md).
Реконструкция теперь содержит actual shared-core read/index/write и runtime;
11 native/source сравнений совпали. Следующий список — историческая граница
первого serializer-разбора; пункты, закрытые CP20 для PC, не считаются вновь
неизвестными. Для PS2 отдельной runtime-проверки в CP20 не было.

- оригинальные header/source paths и имена методов;
- полный `spAnimTexController` lifecycle/layout вне подтверждённых offsets;
- ownership/refcount различия relationship-массивов PC и PS2;
- предел frame count, обработка integer overflow и allocation failure;
- проверяет ли runtime монотонность/конечность времени после десериализации;
- loop/clamp/restart и правило выбора кадра;
- rollback частично созданных inline textures;
- контролируемый in-game mutation test.

Следующий соседний serializer-кандидат — `spUVControllerSerializer`; до
реконструкции нужно отдельно подтвердить обе регистрации и не переносить на него
грамматику animated textures.
