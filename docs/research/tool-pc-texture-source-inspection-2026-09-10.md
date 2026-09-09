# Общий PC TextureData source reader

Для PC-контейнеров (`platformMask & 2`) `SmoTextureDataDecoder` теперь получает
результат настоящего `spDXTextureDataSerializer::ReadPayloadForAnalysis` на
принадлежащем вызову `spDXTexture`. C# больше не определяет порядок PC
source/derived полей и не пробует PS2 после неудачи DX. Это отдельный переход
от прежнего чтения только leaf sections к выполнению source dispatch.

## Что наблюдается

Опциональный observer записывает source/derived поля в порядке исполнения:
границы frame/field, глубину рекурсии, platform до/после, результат read/skip и
изменение handled. Cross/native представления сохраняют исходные mip offsets
до генерации runtime levels. Пропущенный field0/1 остаётся явно opaque; он не
превращается в отсутствующую вторую копию текстуры.

`InspectPayloadForAnalysis` вызывает тот же virtual reader, сохраняет настоящий
порядок инициализации и восстанавливает context observer при выходе/исключении.
Успех требует полного cursor, прежней глубины, отсутствия read failure и полного
наблюдения. Caps observer: 65536 полей, 4096 представлений, 65536 stored mips.
Достижение cap делает inspection incomplete, не меняя ветви обычного runtime.
Изменений оригинального алгоритма загрузки или mip-фильтров нет.

C ABI заимствует вход только на время вызова. После копирования metadata
настоящий CPU owner и mip buffers уничтожаются. Managed cache на immutable
document/entry повторно использует metadata, не удерживая native owner и не
повторяя генерацию mips при каждом открытии инспектора.

## Потребители и запись

`SelectedRepresentation` обозначает результат последней успешной инициализации;
`ObservedRepresentations` сохраняет порядок. `HasOpaqueRepresentations`
предупреждает о непрочитанных source fields. Прежние CrossPlatform/PlatformSpecific
остаются краткими metadata-проекциями, а не правилом выбора preview. Viewer
texture preview и TextureTool теперь используют выбранное представление.
`RuntimeMipLevelCount` отделяет generated levels от сохранённых.

`EditingIssue` по умолчанию запрещает неподтверждённую операцию. Проверенный
старый embedded `[3,1,0]` путь остаётся доступным для одного BGRA mip; необычные,
повторные или skipped source-поля не открывают запись автоматически. Проверка
source shape использует native observations; внутреннюю неоднозначность mip
по-прежнему проверяет lossless editor. FAT/envelope writer этим не перенесён.

Legacy-common и PS2 metadata имеют `UsesRuntimeSourceSelection=false`, неизвестное
число runtime mips и read-only ограничения. Для них старое ограниченное чтение
source-оболочки ещё не заменено игровым runtime. Разбор PS2 native section
перенесён [отдельно](tool-ps2-texture-native-inspection-2026-09-10.md).
[Book boundary](tool-legacy-texture-source-boundary-2026-09-10.md) объясняет,
почему original bool-success без Init не разрешает выдумать рабочую текстуру.
Исторический combined TextureData Corpus profile остановлен перед открытием
БД для записи; для новых PC source и PS2 metadata нужны раздельные profiles.

## Адресная проверка

- Native TextureSerialization **1219**, FullLoader **213**, PS2 metadata **204**
  assertions прошли. Это счётчики проверок, не число новых готовых операций.
- Изменённый source повторно сопоставлен с сохранённым original CP119 capture
  icebat: **5460 mip bytes**, один stored и шесть runtime levels, cursor4152.
  Новый запуск original guest не потребовался.
- Managed **58 checks**: четыре реальные текстуры book/Bloom_body/icebat и
  десять небольших source-форм. Проверены raw hashes/offsets, selection,
  skipped cross, repeated fields, source-none local, запрет неподтверждённой
  записи, malformed extent и foreign entry.
- Общая проверка Viewer: **610 assertions**; локальный Samples отсутствует,
  поэтому corpus часть не выполнялась. FormatTests собраны без warnings/errors.
- TextureTool: **71 checks** при экспорте трёх PNG из двух SMO, затем
  **2052 assertions** для шести вариантов замены на одном Bloom_body.
  Три PNG побайтно совпали с прежней сборкой. Это адресная проверка
  существующего editor path, не завершение FAT/envelope migration.

Первый cap test ошибочно превысил существующий лимит одной секции; исправлен
только fixture, production не менялся. Старый managed test считал byte02
неверным terminator. Общий оригинальный header reader допускает нулевой size
form независимо от low field bits: добавлен положительный случай02, а реальный
truncated field проверяется byte22. Reader под старое ожидание не менялся.

До checkpoint исправлен дефект нового preview adapter: повторное чтение всей
cross section для каждого XRGB observation выбирало последний field5. Теперь
каждый observation проецирует собственные stored pixels через общий raw codec.
Один bridge helper также заменяет два прежних одинаковых XRGB loops. Проверены
раздельные BGR/raw alpha и opaque preview двух повторных записей. Новый тест
сначала неверно ожидал один runtime mip для stored1×1: original normalization
даёт2×2 и два уровня; исправлен только test expectation.

Артефакты: `local-data/results/tools-core-cycle-20260910-0730/texture-source/`.
Итоговые checkpoint записываются в [журнале цикла](tools-core-cycle-2026-09-10-0730.md).
