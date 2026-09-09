# Importer: платформа назначения нового TextureData

Importer создаёт новые текстуры общим PC DX writer. Теперь template selection,
замена по шаблону и canonical path требуют PC-compatible контейнер
(`platformMask & 2`); skinned branch использует ту же проверку. Отказ
`TEXTURE_DESTINATION_PLATFORM` происходит до сериализации и чтения импортируемого
изображения. UI не менялся, игровой writer не переписывался.

Причина: старый importer выбирал legacy-common book как допустимый шаблон и
помещал native PC wrapper в тот же platform1 контейнер. Прежнее metadata-чтение
такого результата не доказывало правильную фабрику/Init. После подключения
actual source reader шесть старых level/skin/canonical проверок book выявили
`Written object no longer decodes`; все обычные PC операции Bloom_body прошли.
Правила factory/source разобраны [отдельно](tool-legacy-texture-source-boundary-2026-09-10.md).
Это не утверждение, что original game вообще не может читать book.

Удалены два недостижимых legacy template special cases; произвольная замена
platform mask не добавлена. Преобразование всего legacy-common/PS2 контейнера
в PC потребует отдельной согласованной операции и проверки фактической загрузки.
Этот вариант импорта остаётся приостановлен; обычная PC запись сохранена.

Проверка после исправления на book/Bloom_body:42 успешных template/import checks,
9 guards неподдерживаемых source shapes и4 ранних отказа legacy destination.
Для двух PC texture templates проверены original/1×1/13×7/compact wrappers,
импорт8×8/17×9, level/skin/canonical consumers. Исходные файлы неизменны;
Все34 PC output SMO побайтно совпали с состоянием до этой host-проверки.
результаты находятся только в локальном каталоге. Сборка FormatTests чистая.
Новая проверка не изменяет несогласованный FAT/envelope writer.

Старый unsupported-template test запускался на book и ошибочно требовал
структурного PC decode всех native fixtures в platform1; guard fixtures теперь
используют настоящий PC specimen. Bare cross source в PC явно отклоняется
reader-ом вместо прежнего заявления о его инициализации.

Evidence: `local-data/results/tools-core-cycle-20260910-0730/importer-texture-destination/`.
