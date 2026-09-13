# spTextureBuffer

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spTextureBuffer](../../../Sparkplug/Code/Sparkplug/spTextureBuffer.h).

Статус: общий class ID/direct base, concrete factory, exact layout `0x30`,
constructor state, ownership двух указателей, `Init`, таблица pixel size и blank
RTTI clone подтверждены на PC и PS2. Исходный тип вспомогательного объекта
`+0x24`, имя третьего измерения и enum pixel format пока неизвестны.

| Platform | PC | PS2 |
| --- | ---: | ---: |
| Class ID / base | `0x205B390B / spBaseObject` | same |
| `Init` | `0x00475F30` | `0x00173C90` |
| Exact size | embedded boundary `spTextureData +0x38..+0x67` | factory allocation `0x30` |

## Layout и lifetime

Обе платформы используют одинаковый 32-битный layout:

| Offset | Size | Подтверждённая роль |
| ---: | ---: | --- |
| `+0x00` | `0x10` | `spBaseObject` |
| `+0x10` | 4 | width; вход `Init` читается как `u16` |
| `+0x14` | 4 | height; вход `Init` читается как `u16` |
| `+0x18` | 4 | третье измерение; вход `Init` читается как `u16` |
| `+0x1C` | 4 | owned raw pixel buffer |
| `+0x20` | 4 | pixel format |
| `+0x24` | 4 | optional owned polymorphic object неизвестного типа |
| `+0x28` | 4 | pixel size в байтах |
| `+0x2C` | 1 | initialized |

PS2 factory непосредственно выделяет `0x30`. На PC окончание встроенного
`spTextureBuffer` доказывается следующим членом `spTextureData` по `+0x68`, а
доступы, destructor и vtable совпадают с PS2.

Constructor задаёт нули в `+0x10..+0x1C`, `+0x24/+0x28/+0x2C`, но
`pixelFormat +0x20 = 6`. Destructor освобождает `+0x1C`, вызывает deleting
destructor объекта `+0x24`, обнуляет оба указателя и initialized. Остальные
scalar-поля остаются нетронутыми.

RTTI clone создаёт новый объект через factory, регистрирует пару в clone
manager и вызывает только inherited copy slot `spBaseObject`. Размеры, формат,
указатели и payload в clone не копируются — результат остаётся constructor-blank.

## `Init` и форматы

Точная source-level форма пяти аргументов целиком не сохранилась. Обе реализации
принимают после `this` три `u16`, pointer и pixel-format word. Serializer
буквально показывает практический вызов
`Init( uWidth, uHeight, 1, NULL, epfPixelFormat)`; поэтому третий аргумент в
portable API назван `depth` только аналитически.

Алгоритм обеих платформ одинаков:

1. освободить прежние `+0x1C/+0x24`, очистить initialized;
2. записать width, height, третий аргумент, pointer и format;
3. сопоставить format с размером пикселя;
4. выделить `width * height * depth * pixelSize` байт;
5. записать raw pointer и установить initialized.

| Format value | Pixel size |
| ---: | ---: |
| `0`, `1` | 4 |
| `2` | 1 |
| `3`, `4` | 2 |
| `>4` | ошибка |

На PC соответствие подтверждается jump table `0x00475FD0`; PS2 содержит те же
ветви. Неверный формат уже уничтожил старый payload и обновил dimensions/format,
но не перезаписал `+0x28`: прежний pixel size остаётся наблюдаемым. Portable
срез сохраняет это странное состояние, однако отклоняет переполнение итогового
размера до allocation. Нативный код умножает в 32 битах без такой защиты.

Диагностические строки `spTextureDataSerializer` сохраняют настоящие имена
`GetTextureBuffer`, `GetWidth`, `GetHeight`, `GetPixelFormat`, `GetPixelSize` и
`GetBuffer`. Они доказывают API чтения и порядок сериализуемых полей, но не имя
третьего измерения и не тип объекта `+0x24`.

## Границы описания

`Code/Sparkplug/spTextureBuffer.*` реализует concrete RTTI/factory, blank clone,
проверяемую часть `Init`, exact-size payload и безопасный release. Объект
`+0x24` намеренно не создаётся и не моделируется фиктивным `void*` API.

Остаётся найти original header/TU, имена pixel-format enum и третьего аргумента,
тип и назначение `+0x24`, отдельный deep-copy контракт (если он существует), а
также связь с platform texture upload. Ближайший источник этих ответов —
`spTextureData` и его serializer.
