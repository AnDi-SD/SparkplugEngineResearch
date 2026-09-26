# wxAnimationManager

Хранит ресурсы анимации по разрешённому имени и 68 таблиц соответствия packed key → resource. Загружает текстовые `.anm`, преобразует поля строки в ключ, получает ресурс и выполняет поиск с fallback. Class ID `9816352C`, регистрационная база — `spBaseObject` (`415352A1`).

Исходники: [wxAnimationManager.cpp](../../../Winx/Code/wxAnimationManager.cpp), [интерфейс](../../../Winx/Code/wxAnimationManager.h). Файлы, потоки и загрузка самих ресурсов подключаются через обязательный [host](../../../Winx/Analysis/Host/wxAnimationManagerHost.h). Контейнеры переносимого класса — `std::map`; они не заявляют бинарную совместимость с оригинальными деревьями.

## Устройство объекта

Адреса и смещения здесь шестнадцатеричные; число таблиц и их индексы — десятичные.

| Поле / операция | PC | PS2 |
| --- | --- | --- |
| Размер | `350` | `464` |
| Основная vtable | `703470` | `492FF0` |
| Вторичная vtable по `10` | `70346C` | `493014` |
| Кэш по имени | `14`, размер `0C` | `14`, размер `10` |
| Массив 68 таблиц | `20`, шаг `0C` | `24`, шаг `10` |
| Constructor | `59C970` | Встроен в factory |
| Factory | `59CA40` | `274650` |
| Destructor | `59B6B0` | `274250` |
| Clone | `59CAA0` | `274520` |
| Загрузка набора | `59BCB0` | `272D50` |
| Получение ресурса по имени | `59B780` | `273E20` |
| Освобождение набора | `59AE80` | `273BF0` |
| Поиск по ключу | `59A5E0` | `273D10` |
| Очистка всех ресурсов и таблиц | `59B190` | Включена в destructor |

Точные размеры отделены от переносимого класса: [PC ABI](../../../Winx/Analysis/PC/wxAnimationManagerAbi.h), [PS2 ABI](../../../Winx/Analysis/PS2/wxAnimationManagerAbi.h). Вторичный подобъект управляет singleton; его исходное имя не установлено. Это физическая особенность, отсутствующая в регистрационной цепочке RTTI.

Constructor создаёт пустой кэш и 68 пустых таблиц, записывает себя в singleton. Clone создаёт такой же пустой менеджер, регистрирует соответствие в clone manager и вызывает inherited Copy. Copy базы не переносит содержимое. Создание clone заменяет singleton; destructor любого экземпляра безусловно обнуляет его, даже если до этого singleton уже указывал на другой экземпляр.

## Кэш и владение

`GetAnimationForAnalysis(name, set)` сначала вызывает внешний resolver пути с category/group данного набора и ёмкостью буфера `0x136` (310 байт). Даже при попадании в кэш resolver вызывается снова. Затем выполняется точный, чувствительный к регистру поиск полученной C-строки.

При попадании возвращается сохранённый ресурс. При промахе внешний loader получает путь; его результат, включая null, записывается вместе с ID набора. Неудачная загрузка поэтому не повторяется до удаления записи. ID владельца назначается при первой вставке и не меняется при обращении из другого набора. Исходный тип возвращаемого указателя скрыт переносимым `Resource = void*`; adapter сохраняет его идентичность.

`ReleaseSetForAnalysis(set)` обходит кэш в порядке имён, уничтожает ненулевые ресурсы с соответствующим owner и удаляет эти записи. Затем очищает только индексированную таблицу данного набора. Ссылки в других таблицах не переписываются: если разные наборы разрешились в одно имя, освобождение первого владельца может оставить в другой таблице прежний указатель. Реконструкция не добавляет отсутствующий refcount.

`ClearForAnalysis()` уничтожает все ненулевые ресурсы в порядке кэша, очищает кэш, затем все 68 таблиц. Повторная очистка пуста. Destructor сначала выполняет эту операцию, уничтожает контейнеры, обнуляет singleton и завершает базовый объект.

## Загрузка `.anm`

Для обычного набора выбираются имя таблицы и group. Отсутствующее имя означает ранний выход. Набор 66 выбирает имя по текущему уровню; при неизвестном уровне имя **пустое**, но вызов открытия всё равно выполняется. Наборы 10, 19, 21, 22, 26, 49, 67 и значения вне 0–67 не загружают `.anm` этой операцией.

Открытие передаёт `(name, category=10, group, flags=0)`. При null источнике метод возвращается, сохраняя прежнее содержимое. Для открытого источника создаётся поток `TempAnimationMap`, в него копируется источник и выполняется seek `(1,0)`. Эти внешние операции объединены в `CreateTokenStreamForAnalysis`.

Каждый проход читает **все восемь полей**, затем сравнивает первое поле с `end`. Совпадение прекращает цикл. Остальные строки преобразуются в ключ; ресурс получается по восьмому полю и записывается в таблицу набора. Повторный ключ заменяет значение, но старый ресурс остаётся в кэше до явного освобождения. Существующая таблица перед чтением не очищается.

| № | Поле | Ёмкость native буфера, байт | Разделитель | Биты ключа |
| --- | --- | --- | --- | --- |
| 1 | Mode | 20 | `,` | 0–3 |
| 2 | Direction | 20 | `,` | 4–6 |
| 3 | Action | 40 | `,` | 7–14 |
| 4 | Status | 20 | `,` | 15–18 |
| 5 | Jump | 10 | `,` | 19–20 |
| 6 | Phase | 20 | `,` | 21–22 |
| 7 | Variant | 5 | `,` | 23–27 |
| 8 | Имя анимации | 20 | `;` | Не входит в ключ |

Сначала закрывается и уничтожается исходный поток, затем `TempAnimationMap`. Нативный token reader остаётся внешней зависимостью: host должен воспроизводить разделители, ограничения буферов и терминатор. `LoadSetForAnalysis` не заменяет неизвестное поведение повреждённого файла придуманным успешным EOF.

Слова распознаются с учётом регистра. Неизвестное слово даёт 0. Variant допускает ровно строки `0`…`16`; `01`, `17` и произвольные числа не разбираются как integer. Jump: `none=0`, `jumping=1`, `falling=2`. Phase: `main=0`, `enter=1`, `exit=2`.

| Поле | Соответствия token → value |
| --- | --- |
| Mode | `moving=0`, `flying=1`, `strafing=2`, `crouching=3`, `hanging=4`, `ladders=6`, `vines=7`, `sticking=5`, `dialogue=8`, `dating=9` |
| Direction | `none=0`, `up=1`, `down=2`, `left=3`, `right=4`, `all=5` |
| Action | `none=0`, `blast=1`, `missile=2`, `defend=3`, `kick=4`, `chest=5`, `stairs=6`, `waytogo=24`, `dispel=25`, `door=7`, `attack1=8`, `attack2=9`, `attack3=10`, `attack4=11`, `attack5=12`, `glasses=13`, `shaketree=14`, `openchest=15`, `action=16`, `try2hoist=17`, `pulllever=19`, `pickflower=20`, `readsign=18`, `opengate=21`, `opensecret=26`, `neutralTalking=28`, `neutralListening=29`, `happyTalking=30`, `happyListening=31`, `angryTalking=32`, `angryListening=33`, `discouragedTalking=34`, `discouragedListening=35`, `convaincedTalking=36`, `convaincedListening=37`, `bedIdle=39`, `bedTalking=40`, `bedListening=41`, `chairIdle=42`, `chairTalking=43`, `chairListening=44`, `idleLyingDialogue=47`, `lyingTalking=45`, `lyingListening=46`, `hoverTalking=48`, `hoverListening=49`, `idleHoverDialogue=50`, `idleChairDialogue=51`, `idleBedDialogue=52`, `idleDialogue=53`, `thinkingDialogue=38`, `idleMedDialogue=54`, `idleLong1Dialogue=55`, `idleLong2Dialogue=56`, `idleLong3Dialogue=57`, `glyph=22` |
| Status | `none=0`, `hurt=1`, `stunned=2`, `dead=3`, `struggling=4` |

В частности, оригинальная орфография `convaincedTalking` и `convaincedListening` сохранена. Точное распределение action приведено выше; enum не восстанавливается по порядку названий.

## Поиск

1. Найти точный key в выбранной таблице.
2. Только при промахе в таблице 0 — найти тот же key в таблице 66.
3. Если запись всё ещё отсутствует — найти key 0 в первоначальной таблице.

Нулевая величина найденной записи является результатом, а не промахом. При отсутствии обязательного default оригинал читает payload end iterator; подтверждённого значения ошибки нет. Переносимый API явно бросает `logic_error` вместо выдуманного результата. Для `Select` и `ReleaseSet` индекс вне 0–67 отвергается `out_of_range`; это наш guard перед недопустимым native обращением.

Native устройство деревьев и порядок fallback описаны в [контракте поиска](../../game/characters/character-animation-selection.md).

## Таблицы PC

Все числа в следующих таблицах десятичные. Category/group пути применяются к прямому получению ресурса; они могут существовать у набора, для которого нет `.anm`. Для прямого получения ресурса с ID вне 0–67 используются category 2, group 0.

| Set | Имя `.anm` | Group `.anm` | Category / group пути |
| --- | --- | --- | --- |
| 0 | `Bloom.anm` | 1 | 2 / 1 |
| 1 | `BloomX.anm` | 2 | 2 / 2 |
| 2 | `Ghoulie.anm` | 5 | 2 / 5 |
| 3 | `Bird.anm` | 6 | 2 / 6 |
| 4 | `Knut.anm` | 7 | 2 / 7 |
| 5 | `Kiko.anm` | 8 | 2 / 8 |
| 6 | `Iceworm.anm` | 11 | 2 / 11 |
| 7 | `IceGargoyle.anm` | 12 | 2 / 12 |
| 8 | `IceBat.anm` | 6 | 2 / 6 |
| 9 | `Spirit.anm` | 13 | 2 / 13 |
| 10 | — | — | 12 / 0 |
| 11 | `Yeti.anm` | 14 | 2 / 14 |
| 12 | `Mosquito.anm` | 15 | 2 / 15 |
| 13 | `HungryHopper.anm` | 16 | 2 / 16 |
| 14 | `HairySpider.anm` | 17 | 2 / 17 |
| 15 | `Troll.anm` | 18 | 2 / 18 |
| 16 | `Golem.anm` | 19 | 2 / 19 |
| 17 | `Baco.anm` | 20 | 2 / 20 |
| 18 | `Minotaur.anm` | 21 | 2 / 21 |
| 19 | — | — | 14 / 0 |
| 20 | `GoopMonster.anm` | 24 | 2 / 24 |
| 21 | — | — | 2 / 26 |
| 22 | — | — | 4 / 0 |
| 23 | `Book.anm` | 6 | 2 / 6 |
| 24 | `Butterfly.anm` | 6 | 2 / 6 |
| 25 | `Fish.anm` | 6 | 2 / 6 |
| 26 | — | — | 4 / 0 |
| 27 | `Specialist.anm` | 40 | 2 / 40 |
| 28 | `Faragonda.anm` | 30 | 2 / 30 |
| 29 | `Griffin.anm` | 32 | 2 / 32 |
| 30 | `WinxStudents.anm` | 28 | 2 / 28 |
| 31 | `Darcy.anm` | 29 | 2 / 29 |
| 32 | `WinxStudents.anm` | 31 | 2 / 31 |
| 33 | `Grizelda.anm` | 33 | 2 / 33 |
| 34 | `Icy.anm` | 34 | 2 / 34 |
| 35 | `WinxStudents.anm` | 35 | 2 / 35 |
| 36 | `WinxStudents.anm` | 36 | 2 / 36 |
| 37 | `WinxStudents.anm` | 37 | 2 / 37 |
| 38 | `Palladium.anm` | 38 | 2 / 38 |
| 39 | `WinxStudents.anm` | 41 | 2 / 41 |
| 40 | `Stormy.anm` | 42 | 2 / 42 |
| 41 | `WinxStudents.anm` | 43 | 2 / 43 |
| 42 | `Wizgiz.anm` | 44 | 2 / 44 |
| 43 | `WinxStudents.anm` | 45 | 2 / 45 |
| 44 | `XWinx.anm` | 46 | 2 / 46 |
| 45 | `XWinx.anm` | 47 | 2 / 47 |
| 46 | `XWinx.anm` | 49 | 2 / 49 |
| 47 | `XWinx.anm` | 48 | 2 / 48 |
| 48 | `Shadowbeast.anm` | 52 | 2 / 52 |
| 49 | — | — | 14 / 0 |
| 50 | `WinxStudents.anm` | 39 | 2 / 39 |
| 51 | `DatingBloom.anm` | 3 | 2 / 3 |
| 52 | `DatingSky.anm` | 3 | 2 / 3 |
| 53 | `Droid.anm` | 53 | 2 / 53 |
| 54 | `Guardian.anm` | 54 | 2 / 54 |
| 55 | `Daphne.anm` | 55 | 2 / 55 |
| 56 | `WinxStudents.anm` | 57 | 2 / 57 |
| 57 | `WinxStudents.anm` | 58 | 2 / 58 |
| 58 | `WinxStudents.anm` | 59 | 2 / 59 |
| 59 | `KnutFriend.anm` | 7 | 2 / 7 |
| 60 | `UpsieDaisySpider.anm` | 17 | 2 / 17 |
| 61 | `Dragon.anm` | 60 | 2 / 60 |
| 62 | `UpsieDaisyGuard.anm` | 40 | 2 / 40 |
| 63 | `Mikael.anm` | 63 | 2 / 63 |
| 64 | `WinxStudents.anm` | 66 | 2 / 66 |
| 65 | `Saladin.anm` | 67 | 2 / 67 |
| 66 | По уровню | 1 | 2 / 1 |
| 67 | — | — | 4 / 0 |

Набор 66 всегда использует group 1; имя `.anm` зависит от уровня:

| Уровни | Имя |
| --- | --- |
| 1 | `Gardenia1Bloom.anm` |
| 2 | `Gardenia2Bloom.anm` |
| 3 | `Gardenia3Bloom.anm` |
| 4, 5 | `Domino12Bloom.anm` |
| 6 | `Domino3Bloom.anm` |
| 7, 8 | `Domino45Bloom.anm` |
| 9, 10, 11, 12, 13 | `SwampBloom.anm` |
| 14 | `CT1_1Bloom.anm` |
| 15, 16 | `CT1_23Bloom.anm` |
| 17, 18, 21, 22 | `CT2_125Bloom.anm` |
| 19 | `CT2_3Bloom.anm` |
| 20 | `CT2_4Bloom.anm` |
| 23, 25, 26 | `RF134Bloom.anm` |
| 24 | `RF2Bloom.anm` |
| 27, 28, 31, 33, 34 | `AlfeaBloom.anm` |
| 29, 32, 35 | `AlfeaLadderBloom.anm` |
| 30 | `AlfeaFightBloom.anm` |
| 36, 37 | `SkyBloom.anm` |
| 41 | `Star1Bloom.anm` |
| 42 | `Star2Bloom.anm` |
| 43 | `Star3Bloom.anm` |
| 44 | `Race1Bloom.anm` |
| 45 | `Race2Bloom.anm` |
| 46 | `Race3Bloom.anm` |
| 47 | `Battle1Bloom.anm` |
| 48 | `Battle2Bloom.anm` |
| 49 | `Battle3Bloom.anm` |
| Прочие | Пустая строка |

## Подключение к загрузчику и границы

[wxAnimationLoaderManagerHost](../../../Winx/Analysis/Host/wxAnimationLoaderManagerHost.h) связывает [wxAnimationLoader](wx-animation-loader.md) с общей реализацией менеджера. Приложение предоставляет entity, текущий уровень и разрешение singleton; сам адаптер передаёт LoadSet/ReleaseSet менеджеру без второй копии логики. Менеджер должен жить дольше загрузчика. Вызовы адаптера `noexcept` соответствуют существующему интерфейсу загрузчика; исключения внешнего host нельзя пропускать через эту границу.

Переносимые контейнеры, обязательный host, исключения для неверных индексов/default и cleanup потоков при C++-исключении являются нашими техническими адаптациями. Изменение менеджера из callback уничтожения ресурсов не входит в установленный контракт.

Восстановлена собственная логика PC. PS2 layout, поиск и освобождение имеют независимое описание; полное выполнение её загрузчика и тождество всех строковых таблиц между платформами здесь не заявляются. Файловая система, resolver, декодирование ресурса анимации и реализация token reader остаются внешними системами.
