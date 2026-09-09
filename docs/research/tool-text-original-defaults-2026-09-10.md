# Text: original defaults и байтовая строка

Узкий следующий шаг после [статического аудита](tool-text-inspection-audit-2026-09-09.md).
Пять fresh PC micro guests, без новых seams, повышения caps, resume,
production-правок, сборок и corpus sweep. Прежние пределы: 100k инструкций/2s
на вызов, arena64KiB, single allocation32KiB, child30s.

## Подтверждено по оригиналу

`spTextRenderable` factory **41A640** завершилась и вернула объект **A4B**,
vtable **6DEC60**. При allocator-маркере CC: text+5C=null, font+60=null,
color+64=FFFFFFFF, wrap+68=0, alignment+80=0. Область +8C..+A0 остаётся CC;
не выдавать её за вычисленные constructor bounds. Deleting destructor4380D0
освободил объект; lazy56B allocation остался вне проверенного lifetime scope.

`spTextNode` factory **41A5E0** завершилась: **1DCB**, vtable **6DEC18**,
40,023 инструкции. Destructor437A70 остановился в RenderSupport469CA9 на
отсутствующем renderer cache (адрес C190). Factory defaults доступны в полном
snapshot; чистый teardown и дополнительные значения TextNode не объявляются
подтверждёнными этим запуском. Нового parser для inherited sections не нужно:
прямые переходы в RenderNode serializer уже установлены предыдущим аудитом.

Setter **4380F0** с непустой строкой и factory-default font=null остановился
в layout437F77 при чтении height через null Font. Это конкретная зависимость,
не причина подменять layout или обходить fault. Исходный guest не продолжался.

Для нового входа Font создан original factory462EC0 и прочитан actual442660:
height12, baseline3, ширины A=5/B=7, atlas=null. Ссылка Text+60 и один owning
refcount Font+8 **явно заданы fixture** по уже подтверждённой форме read-loop;
вызов оригинального font-assignment setter этим не доказывается.
Setter4380F0 с аргументами `AB\0, 1` полностью завершился (8,507 инструкций):
отдельное allocation3B содержит **414200**, font refcount сохранился1,
вычисленная оригиналом ширина+84=12. Это raw byte string, включая завершающий
ноль; wire byteCount такого значения был бы3, не чётная UTF-16 длина.
Original writer в данной пробе не вызывался, кодировка отображения не выбиралась.
Actual Text destructor освободил объект, копию строки и Font; serializer тоже
освобождён. Infrastructure allocations4132B/56B/60B остались вне этого scope.

## Дополнительный Font reader факт для текущей интеграции

Одна проба **442660** прочитала два field0, каждый с настоящим null-ID atlas
prefix и 224 packed17B glyphs. Reader вернул true за35,470 инструкций,
потребил все7651B, diagnostics отсутствуют. Последнее присваивание победило:
heightFFFFFFFF, baselineFFFFFFFE, glyph width255-i; все UV bits
**7FC12345/FF800000/80000000/7F800000** сохранены точно (NaN/-Inf/-0/+Inf).
Все224 записи совпадают с actual внутренним20B layout. Никакого finite/UV-range
правила оригинальный reader на этих входах не применил.

`font-reader.json` честно сохраняет общий status=blocked: после успешных reader
и обоих destructors слишком широкий `all allocations freed` assertion обнаружил
4132B allocation общей инфраструктуры. Это ограничивает lifetime proof, не
отменяет записанные возврат reader, exact cursor и raw-state comparison.
Повторной пробы ради смены статуса не было. Предварительный tiny-field helper
отказался от большого glyph payload до создания guest; исправлен только
fixture envelope на уже известный UInt32 size code.

## Артефакты и границы

Все результаты находятся в
[`local-data/results/tools-core-cycle-20260910-0730/text-original-defaults/`](../../local-data/results/tools-core-cycle-20260910-0730/text-original-defaults/):

- `text-factory.json` и snapshot: actual scalar defaults и factory/delete.
- `text-setter.json`: сохранённый null-Font fault, не successful setter.
- `textnode-factory.json`: successful factory и отдельный teardown blocker.
- `font-reader.json`, `font-reader-input.bin`, `font-reader-after.bin`: raw/repeat proof.
- `text-with-font.json`, `text-with-font-input.bin`, `text-with-font-after.bin`: completed setter/layout и освобождение явно связанных ресурсов.
- `manifest.json`: hashes результатов, скриптов, досье и dependency verification.

PC SHA256: `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Исторический Font constructor proof и его dependencies перепроверены отдельно.
PS2 просмотрен только полезный registration lookup: TextNode factory134380,
TextRenderable factory134270. Его дальнейшее изучение прекращено после успеха
доступного PC-пути; PS2 runtime-equivalence или setter behavior не заявляются.

Для общего reader теперь известны реальные scalar defaults, byte-string copy
и Font raw-state semantics. Нельзя из этого выводить полную layout-эквивалентность
на всех строках/шрифтах, назначение font через неисполненный setter, общий Unicode
mapping, чистый global teardown или возможность заменить неизвестные side effects
фиктивным успешным результатом. Следующий конкретный срез остаётся общим кодом,
а C# получает его через тонкий bridge.
