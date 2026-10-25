# spRenderTarget

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spRenderTarget](../../../Sparkplug/Code/Sparkplug/spRenderTarget.h).

## Место в графическом конвейере

Исполняемые файлы доказывают три параллельные ветви:

```text
spResource (0x46F043FE)
└─ spRenderTarget (0x00D1229C, abstract)
   ├─ spDXRenderTarget (0x189A4642, PC, abstract)
   │  └─ spPCRenderTarget (0x0C681FC8, concrete)
   └─ spPS2RenderTarget (0x30A7021D, concrete)

spRenderTarget
└─ spCubeRenderTarget (0x0F8B095F, abstract)
   ├─ spDXCubeRenderTarget (0x5249684C, PC, concrete)
   └─ spPS2CubeRenderTarget (0x5AE83884, concrete no-op backend)

spCrossPlatform (0x20A72504)
└─ spRenderTargetManager (0x546C50F2, abstract)
   ├─ spPCRenderTargetManager (0x165C006F, concrete)
   └─ spPS2RenderTargetManager (0x01486E51, concrete)
```

Общие классы имеют null RTTI factory. Конечные platform-классы имеют factory;
у PC промежуточный `spDXRenderTarget` остаётся абстрактным, а
`spDXCubeRenderTarget` уже concrete. Это не симметричная искусственная
иерархия, а фактическая разница registration graph.

## Общий ABI

Обычная цель на обеих платформах имеет один и тот же доказанный префикс:

| Offset | Поле/роль |
| ---: | --- |
| `+0x00` | `spResource` (`0x14` байт) |
| `+0x14` | vptr отдельного target-интерфейса |
| `+0x18` | intrusive relationship на backing texture |
| `+0x1C` | width |
| `+0x20` | height |
| `+0x24` | `eTBPixelFormat` |

Размер этого common-префикса — `0x28`. `spCubeRenderTarget` добавляет ещё одно
texture-отношение по `+0x28`, поэтому его граница — `0x2C`.

PC `spDXRenderTarget` добавляет D3D surface pointer по `+0x28`; наблюдаемый
префикс равен `0x2C`. `spDXCubeRenderTarget` хранит шесть face surface pointers
по `+0x2C..+0x40`, а его наблюдаемая граница равна `0x44`. Защищённые `.rld`
factory не дают назвать эти PC-величины прямым `sizeof`, поэтому ABI-типы имеют
суффикс `ObservedPrefix`.

PS2 даёт прямые allocation constants:

| Класс | Размер |
| --- | ---: |
| `spRenderTarget` | `0x28` |
| `spCubeRenderTarget` | `0x2C` |
| `spPS2RenderTarget` | `0x30` |
| `spPS2CubeRenderTarget` | `0x2C` |
| `spRenderTargetManager` / PS2 leaf | `0x44` |

У `spPS2RenderTarget` по `+0x28` расположен handle texture manager, а байт
активности — по `+0x2C`. Cube leaf не добавляет storage.

## Разные таблицы форматов

Токен `eTBPixelFormat` подтверждён строкой `Invalid eTBPixelFormat.`. Имена
enumerator-ов не сохранились, поэтому реконструкция пока использует нейтральные
`Format0..Format5`.

| Backend | Допустимые значения | Нативное отображение |
| --- | --- | --- |
| DX ordinary | `0, 1, 3, 4, 5` | `0x15, 0x16, 0x17, 0x1A, 0x19` |
| DX cube | `0, 1, 3, 4` | `0x15, 0x16, 0x17, 0x1A` |
| PS2 ordinary | `0, 1, 5` | `0, 1, 10` |

PS2 renderer содержит точную диагностику
`spCubeRenderTarget not supported on PS2`. Concrete PS2 cube class существует,
но его backend-операции являются успешными no-op; наличие RTTI/factory не
означает поддержку аппаратной cube render target.

## Потеря устройства и менеджер

На PC точный reset-путь выглядит так:

```text
spPCRenderer reset
  -> spRenderTargetManager::ReleaseTargetsForDeviceReset()
  -> D3D device reset
  -> spRenderTargetManager::ReinitTargetsForDeviceReset()
```

Менеджер содержит три разных 12-байтных container state:

| Offset | Содержимое |
| ---: | --- |
| `+0x18` | ordinary render targets |
| `+0x24` | cube render targets |
| `+0x30` | layer render-target textures |
| `+0x3C` | поле, инициализируется нулём |
| `+0x40` | поле, инициализируется единицей |

PC body `0x0045CD10` проходит первые два списка и вызывает release-слот; затем
обрабатывает layer-list отдельным helper. Body `0x0045CE70` повторяет обход для
reinit. Строки диагностик отдельно называют `RenderTarget`,
`CubeRenderTarget` и `LayerRenderTarget`.

Удаление обычной цели (`0x0045CBC0`) и cube-цели (`0x0045CBF0`) не стирает
узел из списка: у найденного узла байт `+0x0C` меняется на ноль. Portable manager
сохраняет раздельные active/inactive списки, а также очищает borrowed pointer
после деактивации как явно безопасную host-политику. Третий список уже связан с
[spMaterialRenderTargetTexture](sp-material-render-target-texture.md):
release/reinit проходят его per-pass target vector. Полное native intrusive
владение backing textures ещё не заменено этим host-представлением.

## Original source anchors

На PC сохранились точные пути:

- `Z:\Sparkplug\Code\SparkplugDX\spDXRenderTarget.cpp`;
- `Z:\Sparkplug\Code\SparkplugDX\spDXCubeRenderTarget.cpp`.

На PS2 сохранился basename `spPS2RenderTarget.cpp`, но не полный путь. Поэтому
`Code/SparkplugPS2/spPS2RenderTarget.cpp` соответствует подтверждённому имени,
а расположение каталога остаётся inferred.

## Адреса ключевых операций

| Операция | PC ordinary | PC cube | PS2 ordinary |
| --- | ---: | ---: | ---: |
| `Init` | `0x004CDC70` | `0x004AC580` | `0x002098A0` |
| `ReinitTargetsForDeviceReset` | `0x004CDBE0` | `0x004AC4F0` | `0x00209A20` |
| `ReleaseTargetsForDeviceReset` | `0x004CDD60` | `0x004AC2A0` | `0x00209820` |
| secondary target vtable | `0x006F33F8` | `0x006EF80C` | leaf group `0x00491CC0` |
| primary vtable | `0x006F3408` | `0x006EF81C` | — |
| surface-size helper | `0x004CDB30` | `0x004AC300` | — |

PC-группа из 13 последовательных адресов содержит две таблицы: четыре записи
secondary target-интерфейса и девять записей primary-интерфейса объекта.
Записи `vptr` в объекте подтверждают границу после первых четырёх адресов;
13 записей не являются 13 отдельными target-операциями.

Первые три secondary slots — `Init`, `ReinitTargetsForDeviceReset` и
`ReleaseTargetsForDeviceReset`. Четвёртый переводит interface pointer в complete
object pointer, вычитая `0x14`, и деактивирует цель в ordinary или cube списке
менеджера. Его оригинальное имя не установлено. Primary-группа содержит
destructor, notification, clone, RTTI и resource-операции. Исходные имена
дополнительных resource slots не определяются одним расположением адресов.

## Значение размера backing texture на PC

`0x004CDB30` получает `D3DSURFACE_DESC` обычной level-zero surface;
`0x004AC300` получает тот же descriptor только у cube face zero. Оба метода
используют ширину из descriptor и игнорируют высоту:

| `D3DFORMAT` | Ordinary result |
| --- | --- |
| `0x15`, `0x16` | `width * 4` |
| `0x17`, `0x1A` | `width * 2` |
| DXT1 (`0x31545844`) | `max(1, width >> 2) * 8` |
| DXT3, DXT5 (`0x33545844`, `0x35545844`) | `max(1, width >> 2) * 16` |
| Остальные значения, включая `0x18`, `0x19` | `width` |

Cube result равен ordinary result, умноженному на шесть. Все действия
32-битные, с переполнением по модулю `2^32`; округления DXT вверх нет.
Например, DXT1 при ширине `7` возвращает `8` для ordinary и `48` для cube.
Эти значения передаются initializer-у backing texture. Это не полный объём
изображения, не сумма mip-уровней и не формула `width * height`.

Оригинал не проверяет результат `GetDesc`. Неуспешный HRESULT при всё же
записанном descriptor не меняет арифметику. Если descriptor не заполнен,
оригинальная функция читает неопределённые stack bytes. Общие исходники
сохраняют арифметику и предоставляют
[явный host-reader](../../../Sparkplug/Analysis/Host/spRenderTargetSurfaceHost.h)
для получения initialized descriptor. Отсутствующий reader или незаполненный
descriptor возвращает отказ host-слоя, сохраняя выходной аргумент; успешного
backend по умолчанию у этого нового пути нет.

## Создание и освобождение PC COM-ресурсов

Восстановленные методы `InitializeDeviceTargetForAnalysis`,
`ReleaseDeviceTargetForAnalysis` и `ReinitializeDeviceTargetForAnalysis`
находятся в [общем PC lifecycle](../../../Sparkplug/Code/SparkplugDX/spDXRenderTarget_Device.cpp).
Они принимают отдельное описание существующего target и backing texture из
[spDXTextureResource.h](../../../Sparkplug/Code/SparkplugDX/spDXTextureResource.h).
Это описание полей и ссылок, потребляемых операцией, а не подмена нативного
конструктора или ABI объекта. COM-вызовы остаются явными внешними endpoints;
порядок создания, присоединения, метаданных и освобождения выполняет общий
восстановленный исходник.

Обычный `Init` сначала вызывает release, затем проверяет engine-формат.
Для допустимого формата он создаёт texture с одним уровнем, usage `1`, pool `0`
и пустым shared handle, получает level-zero surface, измеряет её и присоединяет
созданную texture к backing-объекту. После этого публикует width, height и
engine-формат цели. Проверки ненулевых width/height в этом нативном `Init` нет.
Обычный backing сохраняет преобразованный runtime-формат: например,
`D3DFORMAT 0x17` становится значением `6`; неизвестные форматы проходят без
преобразования.

Cube `Init` передаёт устройству только width как размер квадратной texture,
но записывает исходные width и height в метаданные backing и цели. На каждой
из шести итераций он получает очередную surface, повторно измеряет face zero
и заново присоединяет ту же texture. Cube backing сохраняет сырой D3DFORMAT.
Повторное присоединение освобождает предыдущую texture-ссылку, записывает новую,
обновляет поля и делает `AddRef`, затем вызывает построение младших mip-уровней.
Следовательно, перенос присоединения за цикл изменил бы наблюдаемый порядок
COM-вызовов.

Обычный release выполняется только при наличии surface. Он освобождает texture
из backing, затем вызывает собственный reset backing, который освобождает ту
же ссылку ещё раз и очищает поле texture, после чего освобождает surface и
очищает её поле. Cube release проверяет только face zero, освобождает texture,
вызывает cube backing reset и освобождает все шесть surfaces по порядку.
Cube backing reset дополнительно вызывает `Release` сохранённого device и
оставляет поле device неизменным. Ненулевой результат COM `Release` не мешает
очистке полей texture или surface. Эти особенности сохранены, а не заменены
общей политикой RAII.

`Reinit` допускается при ненулевых сохранённых размерах и формате, отличном
от `6`. Он вызывает release, затем `Init`, который вызывает release повторно.
Неподдерживаемый формат нового `Init` возвращает false уже после освобождения
старых ресурсов и сохраняет старые размеры и формат цели.

Нативные тела игнорируют HRESULT создания texture, получения surface и чтения
descriptor. Ошибочный HRESULT с заполненными выходами поэтому продолжает
обычный путь. Если выход не заполнен или равен NULL перед разыменованием,
portable entry возвращает `completed=false`; он не объявляет такой путь
успешным и не выполняет выдуманный rollback. Результат сохраняет созданную
texture и ещё не освобождённые mip-local surfaces для явного учёта вызывающим
host-слоем. Незаданный descriptor отличается от корректного descriptor при
ошибочном HRESULT.

Присоединение и reset backing находятся в
[spDXTextureResource.cpp](../../../Sparkplug/Code/SparkplugDX/spDXTextureResource.cpp).
Cube mip helper получает число уровней, выставляет общий байт обработки,
обходит грани и соседние пары уровней, получает обе surfaces и их размеры,
вызывает внешний `D3DXLoadSurfaceFromSurface` с полными прямоугольниками,
пустыми palettes, filter `4` и color key `0`, затем освобождает source и
destination. В конце общий байт очищается. Вычисления фильтра SDK не заменены
самодельным алгоритмом. Host ограничивает число уровней диапазоном `1..32`
по умолчанию; это не нативный предел.

Если ordinary attachment получает byte count `0`, он вызывает восстановленный
`MeasureDeviceByteCountForAnalysis` (`0x004AAC50`) перед завершающим `AddRef`.
Метод сначала обнуляет поле `+0x48`, получает количество mip-уровней и обходит
каждый уровень по порядку. Для DXT1 размер уровня равен
`max(1, width >> 2) * max(1, height >> 2) * 8`; для DXT3 и DXT5 множитель равен
`16`. Здесь также используется округление вниз, а не вверх. Для остальных
форматов метод блокирует surface с пустым rectangle и flags `0`, прибавляет
`pitch * height`, затем выполняет unlock. Каждый полученный surface
освобождается после измерения. Все суммы и произведения сохраняют младшие
32 бита; пиксельные данные этот метод не читает. Он возвращает true даже при
нулевом количестве уровней, оставляя размер `0`.

HRESULT получения surface, descriptor, блокировки и разблокировки не меняет
этот путь при заполненных выходных данных. Host отдельно представляет
незаполненный descriptor и locked rectangle: такой результат останавливает
операцию как incomplete с сохранением полученной surface для явного учёта.
Ограничение числа уровней по умолчанию равно `32` и принадлежит host-слою.

Область target entry требует стабильных провайдеров без reentry и нулевого
поля backing `+0x40`, содержащего указатель `spPalette`. У cube target два разных native relationships
`+0x18/+0x28` в этом описании должны указывать на один backing. Отдельные
конкретные тела [palette pool, замены владельца и typed attachment](sp-texture.md#pc-palette-индекс-устройства-и-замена-владельца)
связывают nonzero palette с владельцем ordinary/cube texture и её captured
COM-состоянием. Это отдельный вход; target carrier сам не создаёт такой
владелец. Диагностическая ветвь invalid `Reinit` и произвольное aliasing
остаются открытыми. Обычные виртуальные `Init` и CPU mip shadows сохраняют отдельную
portable модель готовности; новые методы сами по себе не подключают live GPU.

## Что остаётся неизвестным

- исходный тип backing/cube texture relationships по `+0x18/+0x28`;
- original name четвёртого secondary target slot и имена дополнительных
  primary resource slots;
- точный PC allocation size там, где factory закрыта `.rld`;
- полное intrusive владение backing textures и их constructor/destructor;
- связь target creation с material passes и фактическими renderer slots 0/1;
- полные имена `eTBPixelFormat` enumerator-ов.

Следующая связная задача для нативного importer/exporter — идти от третьего
списка и backing texture к texture manager/material render-target texture, а
затем замкнуть вызовы renderer slots 0/1 на фактический render pass.
