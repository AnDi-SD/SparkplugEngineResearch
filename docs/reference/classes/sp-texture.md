# spTexture

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spTexture](../../../Sparkplug/Code/Sparkplug/spTexture.h).

Статус: identity, две разные иерархии, abstract renderer boundary, PS2 exact
layout `0x38`, PC observed extent `0x38`, constructor state, null clone, две
перегрузки инициализации и нормализация размеров подтверждены. Сигнатуры
четырёх аппаратных методов `spITexture` и исходные имена большинства полей
пока неизвестны.

| Platform | PC | PS2 |
| --- | ---: | ---: |
| Class ID / registered base | `0x2F281E13 / spNamedObject` | same |
| Layout status | observed complete extent `0x38` | exact `0x38` |

## Две иерархии

Registration record обоих executable объявляет direct base `spNamedObject`
(`0x44DE07FD`). Однако PS2 constructor вызывает `spResource::spResource`
`0x0017CF40`, а destructor — `spResource` deleting destructor `0x0017CEB0`.
PC destructor аналогично передаёт объект в `spResource` destructor
`0x00467A80`. Поэтому исходная модель имела разные графы:

```text
registration: spNamedObject -> spTexture
C++ lifetime: spNamedObject -> spResource -> spTexture
```

Portable-класс наследует `spResource`, но его `spRTTIRecord` намеренно
ссылается на `spNamedObject`. Сглаживание этого различия сломало бы уже
подтверждённую архитектуру — тот же тип разделения ранее найден у `spApp`.

## Layout и abstract backend

Основной объект имеет следующий доказанный 32-битный layout:

| Offset | Size | Наблюдаемая роль |
| ---: | ---: | --- |
| `+0x00` | `0x14` | C++ base `spResource` |
| `+0x14` | 4 | secondary `spITexture` vptr |
| `+0x18` | 4 | состояние, различающее две `Init`-перегрузки |
| `+0x1C` | 1 | byte argument первой перегрузки |
| `+0x20` | 4 | `spITexture::eTextureFlags` |
| `+0x24` | 1 | initialized |
| `+0x28` | 4 | width |
| `+0x2C` | 4 | height |
| `+0x30` | 1 | размеры не изменились при нормализации |
| `+0x31` | 1 | неизвестно; constructor default `0` |
| `+0x34` | 4 | неизвестно; constructor default `0` |

PS2 constructor обнуляет `+0x1C`, `+0x20`, `+0x24`, `+0x30`, `+0x31` и
`+0x34`; `+0x18/+0x28/+0x2C` получают значения при `Init`. PC читаемый хвост
подтверждает те же последние четыре записи. Точный PS2 размер следует не
только из последнего поля: wrapper `spCubeTexture` вызывает этот constructor,
не добавляет storage и три независимых clone/allocation paths выделяют ровно
`0x38` (`0x001F3660` и соседи).

Строка ошибки PC буквально содержит
`pTexture->Init(pTextureBuffer, 1, spITexture::etfDefault, true )`. Она
доказывает имена `spITexture`, `Init`, `eTextureFlags` и enumerator
`etfDefault = 0`. Secondary vtable содержит четыре pure slots в базовом
`spTexture`; concrete `spCubeTexture` заменяет их thunks. Их точные исходные
сигнатуры пока не устанавливаются по одному calling convention.

## Инициализация и нормализация

PC `0x00423250` и PS2 equivalent выполняют одинаковую общую часть:

1. сбрасывают прежний backend через второй `spITexture` slot;
2. записывают аргументы в `+0x1C/+0x20`, размеры буфера в `+0x28/+0x2C` и
   ставят `+0x24 = 1`;
3. по boolean argument вызывают primary virtual нормализации;
4. только в этой ветке записывают `+0x30 = (old dimensions == new dimensions)`;
5. передают исходный буфер первому `spITexture` slot и возвращают его bool.

Нормализатор `0x00423380 / 0x00173960` вызывает общий helper и вычисляет
`1 << exponent`. Для полезного диапазона это следующая степень двойки с
особыми результатами `0 -> 1`, `1 -> 2`, `2 -> 2`. x86 `SHL` и MIPS `SLLV`
маскируют shift count пятью битами, поэтому out-of-domain значение больше
`0x80000000` даёт `1`; portable helper сохраняет даже эту странность.

## Serializer boundary

Найден точный исходный путь
`Z:\Sparkplug\Code\Sparkplug\spTextureDataSerializer.cpp`. Serializer call
site передаёт `(pTextureBuffer, 1, spITexture::etfDefault, true)` через
virtual slot `Init`. Это связывает общий класс с будущим `spTextureData`, но не
делает `spTexture` самостоятельно сериализуемым payload-классом.
Встроенный CPU-контейнер теперь вынесен в отдельную карточку
[`spTextureBuffer`](sp-texture-buffer.md); он не является
аппаратным `spITexture` backend.

## PC palette: индекс устройства и замена владельца

У PC `spDXTexture` и `spDXCubeTexture` поле `+0x40` содержит указатель на
[`spPalette`](../../../Sparkplug/Code/Sparkplug/spPalette.h). Сам palette
хранит device index по `+0x10` и `1024` байта entries по `+0x14`. Constructor
задаёт индекс `0xFFFFFFFF`, но оставляет entries неинициализированными;
нулевой portable storage поэтому не считается готовой palette.

Выдача и возврат индекса находятся в
[spDXRenderer_Palette.cpp](../../../Sparkplug/Code/SparkplugDX/spDXRenderer_Palette.cpp).
`RegisterPaletteForAnalysis` описывает `0x004BB7D0`: при пустом списке свободных
индексов он берёт renderer counter `+0xCA10` и увеличивает его; иначе берёт
первое значение списка `+0xCA14` и удаляет из списка **все** совпадения этого
значения. Например, `[7, 9, 7]` выдаёт индекс `7` и оставляет `[9]`. Затем
выполняется внешний `SetPaletteEntries` на device slot `+0x11C` с этими
`1024` байтами, после чего индекс записывается в palette. HRESULT игнорируется.
Counter увеличивается по модулю `2^32`: значение `0xFFFFFFFF` тоже может быть
выдано, после чего counter становится нулём.

`RecyclePaletteIndexForAnalysis` описывает `0x004BB840`. Индекс, отличный от
`0xFFFFFFFF`, добавляется в конец списка свободных значений. Поле palette при
этом не очищается. Повторный возврат способен создать дубликат, который
последующая выдача удаляет целиком. Это операции над индексами, а не
освобождение COM texture или автоматическое удаление palette.

Состояние counter и порядка списка представлено в
[spDXPalettePool.h](../../../Sparkplug/Code/SparkplugDX/spDXPalettePool.h).
Portable `std::list` сохраняет значения и их порядок; он не является
32-битным ABI нативного контейнера. Host требует инициализированные entries,
стабильный внешний COM provider без reentry и ограничивает размер списка
значением `4096` по умолчанию. Отсутствующий provider, незаданные entries или
исчерпание host-лимита возвращают incomplete, не успешную заглушку.

`ReplaceDevicePaletteForAnalysis` в
[spDXTextureResource.cpp](../../../Sparkplug/Code/SparkplugDX/spDXTextureResource.cpp)
воспроизводит setter `0x004B93C0` для обычной и cube texture. Если прежний
palette существует, setter сначала возвращает renderer-у индекс **нового**
palette, затем удаляет прежний объект и сохраняет новый указатель. Поэтому
замена old index `5` на new index `23` добавляет в свободный список `23`, а не
`5`. Если прежнего palette нет, новый указатель только сохраняется. Сам setter
не вызывает `SetPaletteEntries` и не присваивает новый индекс.

Нативный setter разыменовывает NULL new palette при наличии old palette;
передача того же указателя приводит к сохранению уже удалённого объекта.
Portable entry отказывает в обоих случаях до изменений. Для корректных
различных владельцев используется `unique_ptr`: новый владелец передаётся по
ссылке, прежний удаляется в исходном месте операции. Это явно ограниченная
замена native pointer ownership. Безопасный host-метод `AdoptPaletteForAnalysis`
сохраняет свою отдельную политику. Деструктор portable CPU shadow освобождает
оставшийся palette через RAII; это не эквивалент нативного DX-деструктора,
который оставляет его живым.

`AttachOwnedDeviceTextureForAnalysis` объединяет эти тела с обычным
`0x004ABAC0` и cube `0x004B9630` attachment. Операция освобождает прежнюю COM
texture, публикует новую texture, выполняет замену palette и, если её индекс
равен `0xFFFFFFFF`, вызывает реальную выдачу индекса. Только затем обновляются
размеры, формат, byte count и признаки готовности, после чего выполняется
`AddRef`. У cube далее вызывается построение mip-уровней. Обычный attachment
при нулевом byte count измеряет уровни перед `AddRef`, как описано у
[render targets](sp-render-target.md).

Следовательно, внешний `SetPaletteEntries` наблюдает уже новый texture и
новый palette, но ещё старые размеры и флаги. Вызов использует device текущего
глобального renderer; это отдельный аргумент, который не подменяется
сохранённым backing device `+0x38`. Если новый palette уже имеет индекс,
регистрация пропускается. При замене существующего владельца индекс нового
palette при этом всё равно попадает в свободный список, как требует setter.

Typed entry принимает фактического владельца palette portable-класса и
отдельное captured описание COM-полей. Поле `state.palette` обязано указывать
на тот же объект, что `GetPaletteForAnalysis`; несогласованное описание
отвергается до COM-вызовов. Метаданные captured состояния не записываются
автоматически в отдельный CPU mip shadow. Отсутствующий внешний endpoint
может остановить операцию после уже выполненного префикса: прежняя COM-ссылка
освобождена, новый texture и владелец palette опубликованы, старые размеры
сохранены. Такому результату соответствует incomplete, без выдуманного
отката. Связь с live backend, полные constructor/destructor и произвольное
aliasing этим entry не закрываются.

## Границы описания

`Code/Sparkplug/spTexture.*` содержит:

- правильное разделение C++ и registration base;
- null clone и отсутствие RTTI factory;
- безопасное состояние всех доказанных полей;
- чистый план и применение общей части buffer-инициализации;
- точный арифметический нормализатор без вызовов GPU/PS2 backend.

Не реализованы и не выдуманы: четыре сигнатуры `spITexture`, назначение
`+0x18/+0x1C/+0x31/+0x34`, точный тип texture-buffer wrapper, platform upload,
форматы и lifetime GPU handles. Их следует закрывать на `spTextureData`,
`spDXTexture`, `spPS2Texture` и `spCubeTexture`.
