# spPCApp

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spPCApp](../../../Sparkplug/Code/SparkplugPC/spPCApp.h).

Статус: identity, inheritance, обе vtable, constructor/destructor, верхняя
последовательность `Initialize -> Run -> Shutdown`, message loop и наблюдаемый
префикс полей восстановлены. Имена части методов и полей аналитические. Точный
полный `sizeof` не доказан, поэтому код не выдаёт нижнюю границу `0x84` за
размер класса.

## Identity

Initializer RVA `0x002D5730` заполняет registration `0x00764710`:

| Поле | Значение |
| --- | ---: |
| class name | `spPCApp` |
| class ID | `0x7635EFDE` |
| base | `spApp / 0x391B146A` |

Constructor entry `0x00447E50` вызывает protected constructor `spApp`
`0x004CA150`; читаемый tail начинается в `0x00447EA0`. Это независимо
подтверждает registration edge. Важно: ранее соседняя строка привела к ложной
атрибуции factory `0x004C33C0`; её getter `0x004C33A0` возвращает registration
`0x00764770`, поэтому это `spPCErrorManager`, не `spPCApp`.

## Наблюдаемый layout

Constructor и оконные функции используют следующий точный prefix:

```text
+0x00  spApp                                      0x20
+0x20  HINSTANCE                                  0x04
+0x24  HWND                                       0x04
+0x28  пока не размеченная область                0x28
+0x50  указатель на имя оконного класса           0x04
+0x54  неизвестное поле                           0x04
+0x58  x                                          0x04
+0x5c  y                                          0x04
+0x60  width, default 1024                        0x04
+0x64  height, default 768                        0x04
+0x68..+0x74  adjusted RECT                       0x10
+0x78  Win32 window style, default 0x00CA0000     0x04
+0x7c  ноль в constructor                         0x04
+0x80  ноль в constructor                         0x04
```

`spPCAppObservedPrefixLayout` имеет размер `0x84`; его имя намеренно говорит, что это минимальный доказанный extent. Factory у абстрактного класса отсутствует. Factory наследника `wxPCApp` выделяет `0x2CC`, но это не позволяет без анализа его constructor провести точную границу base/derived полей.

## Lifecycle и vtable

После общих слотов `spApp` таблица содержит:

| Slot | Target | Результат |
| ---: | ---: | --- |
| `+0x20` | `0x004C3230` | получить module handle, задать defaults, зарегистрировать и создать окно |
| `+0x24` | `0x004C2D60` | pre-update, engine41CD50 всегда, graphics41C460 для foreground/owner, иначе 1 ms throttle; true независимо от engine result |
| `+0x28` | `0x004C2D10` | передать управление общему shutdown cascade |
| `+0x2C` | `0x004C2C70` | Win32 message loop |
| `+0x30` | `0x0048EAA0` | no-op pre-update hook |
| `+0x34` | `0x005B7A00` | no-op hook, сигнатура пока неизвестна |
| `+0x38` | `0x004B9100` | вернуть false |
| `+0x3C` | `0x004C2D20` | обнулить output и вернуть false |
| `+0x40` | `0x004233C0` | вернуть false |
| `+0x44` | `0x004D6550` | no-op |
| `+0x48` | `0x004A1BF0` | вернуть null |

Функция `0x004C3040` вызывает виртуально `+0x20`; при успехе `+0x2C`, затем
при успехе `+0x28`. Message loop обрабатывает очередь через PeekMessage-путь,
на `WM_QUIT (0x12)` ставит унаследованный byte `spApp+0x18` и успешно
завершается. Если сообщений нет, вызывается `Update`; false также ставит флаг,
но возвращает failure. Обычные сообщения проходят translate/dispatch.

Window creation `0x004C3130` строит client rectangle из `+0x60/+0x64`,
вызывает AdjustWindowRect, виртуальные hooks для menu/title, CreateWindowEx,
ShowWindow и UpdateWindow. Это side-effect boundary: portable
`vfunc_20_Initialize()` без отдельного Win32 backend возвращает false.

## Открыто

1. Точный полный `sizeof(spPCApp)` и роли полей `+0x28..+0x57`.
2. Original declarations/signatures слотов после `+0x30`.
3. Имена window class/title и precise error-manager edges.
4. Состав общего shutdown cascade `0x004C2D10`.
