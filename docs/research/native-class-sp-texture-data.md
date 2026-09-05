# Нативный класс `spTextureData`

Статус: identity/base, concrete factory, exact platform layouts, embedded
`spTextureBuffer`, constructor/destructor, blank-data clone, CPU-buffer copy и
две platform-dependent container ABI подтверждены на PC и PS2. Полная семантика
двух списков, блока `0x410` и serializer read/write остаётся отдельной задачей.

| Platform | PC | PS2 |
|---|---:|---:|
| SHA-256 | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` | `198313352DBF4FA26FF8C5D509F6783FC32F9B504A627E416323C5FFBBFFE8FE` |
| Class ID / base | `0x78EA082B / spTexture` | same |
| Registration / initializer | `0x0075D488 / 0x006D1D30` | `0x004A8200 / 0x00480E64` |
| Factory / allocation | `0x0041A2D0 / 0x4A0` | `0x00134CB0 / 0x498` |
| Constructor | `0x00435340` (protected entry) | `0x001790E0` |
| Destructor | body `0x00435280`, deleting `0x004353C0` | deleting `0x00178F40` |
| Clone / getter | `0x0041AC50 / 0x00435330` | `0x00134BF0 / 0x00135B10` |
| Buffer-copy implementation | interface entry `0x00435060` | thunk `0x00135D30` → `0x00178CD0` |
| Payload cleanup | interface entry `0x00435140` | thunk `0x00135D20` → `0x00178E60` |
| Primary / interface vtable | `0x006DE950 / 0x006DE940` | headers `0x0048D980 / 0x0048D9A4` |

Исходный путь самого класса не найден. Строка
`Z:\Sparkplug\Code\Sparkplug\spTextureDataSerializer.cpp` относится к
serializer, поэтому reconstructed `Code/Sparkplug/spTextureData.*` явно
помечен как inferred, а не как доказанный original TU.

## Exact layout и platform split

Общий prefix заканчивается одинаково:

| Offset | Size | Подтверждённая роль |
|---:|---:|---|
| `+0x00` | `0x38` | `spTexture` |
| `+0x38` | `0x30` | embedded `spTextureBuffer` |
| `+0x68` | 1 | выбирает один из двух payload-cleanup paths |
| `+0x6C` | platform | первый контейнер записей размером `0x10` |

Дальше компиляторные ABI расходятся:

| Role | PC | PS2 |
|---|---:|---:|
| first container header | `+0x6C`, `0x10` | `+0x6C`, `0x0C` |
| следующий byte flag | `+0x7C` | `+0x78` |
| opaque inline platform state | `+0x80`, `0x410` | `+0x7C`, `0x410` |
| second container header | `+0x490`, `0x10` | `+0x48C`, `0x0C` |
| final `sizeof` | `0x4A0` | `0x498` |

PC container имеет allocator/begin/end/capacity-end, PS2 — три слова
capacity-or-high-water/count/storage. Это две независимые разницы по четыре
байта и полностью объясняет расхождение итогового размера на восемь байт.
Размер и границы блока `0x410` доказаны, но его поля пока не названы.

PS2 constructor явно вызывает `spTexture`, ставит две vtable, конструирует
`spTextureBuffer +0x38`, обнуляет `+0x68`, оба контейнера и byte после первого
контейнера. PC factory непосредственно выделяет `0x4A0`; cleanup-код независимо
подтверждает переведённые offsets обоих контейнеров.

## Копирование CPU texture buffer

`spTexture::Init` передаёт адрес локального `spTextureBuffer*` в virtual slot.
Concrete реализация `spTextureData`:

1. читает source buffer pointer;
2. вызывает `Init` встроенного `spTextureBuffer +0x38` с теми же width, height,
   третьим `u16`, auxiliary pointer и pixel format;
3. вычисляет `width * height * depth * pixelSize`;
4. копирует весь raw payload в `+0x54`, то есть buffer pointer embedded-объекта;
5. возвращает true.

PC `0x00435060` выполняет тот же цикл относительно interface `this +0x14`, а
PS2 thunk сначала возвращает полный `this`, затем идёт в `0x00178CD0`.

Нормализация logical dimensions принадлежит `spTexture` и происходит перед
копированием. Поэтому при source `3×5` и включённой нормализации внешний
`spTexture` хранит `4×8`, но embedded `spTextureBuffer` всё ещё содержит
исходные `3×5` и соответствующие 60 байт BGRA32. Portable-тест закрепляет это
разделение вместо ошибочного resize payload.

Нативный copy slot не проверяет результат внутреннего `Init` перед `memcpy` и
всегда возвращает true. Portable helper требует initialized source, отсутствие
непредставимого auxiliary object, проверяемую allocation и exact payload size;
это намеренная safety-граница, а не утверждение о нативных проверках.

## Clone и cleanup

RTTI clone обеих платформ выделяет полный объект, запускает constructor,
регистрирует пару в clone manager и вызывает inherited copy slot. Этот slot
копирует только shared name из `spNamedObject`. `spTexture` state, embedded
buffer, оба flags, platform block и списки остаются constructor-blank.

Payload cleanup использует `+0x68`:

- при ненулевом flag проходит первый список с шагом `0x10` и освобождает pointer
  записи `+0x0C`;
- при нулевом flag проходит второй список с шагом `0x14` и освобождает pointer
  записи `+0x10`.

Затем destructor уничтожает оба container storage, embedded `spTextureBuffer`
и базовый `spTexture`. Роли остальных record-слов и источник значения `+0x68`
пока не доказаны, поэтому portable class не предлагает API создания мнимых
platform records.

## Связь с сериализованным SMO

Структурная карточка
[`smo-class-sp-texture-data.md`](smo-class-sp-texture-data.md) уже описывает
пять наблюдаемых storage-вариантов, BGRA, Direct3D mip chain и PS2 native
palette/mip payload во всём корпусе. Настоящий class reconstruction добавляет
runtime lifetime и ABI, но пока не сливает эти данные в writer: соответствие
serializer fields внутренним record-полям должно быть доказано call graph.

## Переносимый срез и открытые вопросы

`Code/Sparkplug/spTextureData.*` содержит concrete RTTI/factory, name-only
clone, embedded buffer, два доказанных constructor flags и безопасный вариант
buffer-copy пути. Exact PC/PS2 layouts остаются в `Analysis`, чтобы разница
container ABI не маскировалась host STL.

Открыты: исходные имена flags и record types, layout блока `0x410`, назначение
всех четырёх interface slots, связь обоих списков с cross/platform-specific
serializer sections, rollback при частичном чтении и platform upload lifetime.
