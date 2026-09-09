# Text: необходимые исправления по оригинальному PC-коду

Узкий статический разбор в конце цикла до23:00 МСК. Исследованы24 явно выбранных
участка PC EXE, суммарно5884B с перекрытиями. Пробы исполнения и изменения
production-кода в этом аудите не выполнялись.

## Подтверждённые ошибки нашего decoder

`spTextRenderable` field0 использует общую **байтовую строку**: UInt16 byteCount
и raw bytes (`441CB7 → 416DC0`). Проверка первого символа в writer и проход
Font measurement читают по одному байту. C# ошибочно требует чётную длину и
применяет UTF-16LE. Единственный прежний corpus-текст `30 00` скрывал ошибку.
Универсальная кодировка отображения байтов этим не установлена.

Field2 — **UInt32 ширина переноса в пикселях**: reader `441CFE` читает UInt32
в `+68`, layout передаёт его в Font measurement, где unsigned comparison
сравнивает ширину с суммой целочисленных glyph widths. C# трактует это как
Single и проверяет finiteness. Такую логику нельзя переносить в общий класс.

Text/color/wrap/alignment могут отсутствовать: writer пропускает пустую строку,
цветFFFFFFFF и нулевые wrap/alignment. Reader принимает unknown/repeated fields.
Это доказывает излишнюю строгость прежнего corpus parser, но не заменяет
проверку factory defaults. Неизвестные defaults не получают выдуманных значений.

## Минимальный следующий перенос

| Класс | Нужный срез | Что уже установлено |
| --- | --- | --- |
| `spFont` | atlas reference, height/baseline,224 glyphs | Factory462EC0; reader442660; glyph17B на диске и20B в памяти. Baseline не инициализируется в просмотренном factory body. |
| `spTextRenderable` | raw text, color, UInt32 wrap/alignment, font | Reader441C10/writer441FC0; конструктор и setter4380F0 требуют отдельной bounded проверки, setters вызывают layout. |
| `spTextNode` | concrete identity/lifetime и inherited RenderNode | Read/write/index entries4423D0/4423B0/4423C0 напрямую делегируют общему RenderNode. Новый самостоятельный parser не нужен. |

Общих concrete классов/serializers пока нет. Следующий приоритет — actual data
classes и минимальный reader/inspection, с явной границей непроверенных layout
side effects; полный font renderer для этого не требуется. Затем тонкая ABI
заменит C# parser. Исправление ещё не реализовано и reader counter не увеличен.

PS2 registration locators сохранены из существующей базы; это не factory
addresses и не новая проверка PS2-equivalence. Доступный PC-код уже ответил
на текущие вопросы, отдельный PS2 проход ради формальности не запускался.

Подробности и точные адреса: [локальное досье](../../local-data/results/tools-core-cycle-20260910-0700/text-inspection-audit/notes.md).
Manifest SHA256: `53866B73B6D6BA4C54772CA65785887AB4DCA4C3FD041E386D364D7A06E9B0A0`.
Pristine PC SHA256: `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
База открывалась read-only; исходные игровые файлы и historical evidence не менялись.

## Дополнение: одна original Font factory проба

После статического аудита выполнен один fresh PC guest: factory462EC0 и
virtual deleting destructor463020, без новых seams и без FontManager/GPU.
При заполнении allocator маркеромCC получены height0, image0 и нулевые224
glyph entries. **Baseline+18 осталсяCCCCCCCC**: original constructor его не
инициализирует. Будущему общему классу нужна явная отметка этого состояния,
а не default0. Это не отсутствие baseline после успешного field0 reader.

Factory4406 инструкций, destructor1696; одно выделение4512B освобождено.
Время0,840с; прежние100k/2s, heap64KiB, single32KiB, child30s сохранены.
Проверены4 зависимости; повторов/повышений caps не было. Завершено22:56:25 МСК.
[Локальные доказательства](../../local-data/results/tools-core-cycle-20260910-0700/font-factory-micro/notes.md).
Original result SHA256: `7AD577B31CEFBA09BA4AFFF35975EF46D74AF4372CF2EBFD5AFCE09BA3ED3EEA`.
Этот результат уточняет следующий перенос и не означает, что Font уже подключён.
