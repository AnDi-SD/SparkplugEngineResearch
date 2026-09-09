# Text: общий serializer для просмотра полей

Текущий потребитель Text — metadata inspector. Восстановлен нужный срез
`spTextRenderableSerializer` PC441C10; полноценная runtime-загрузка Text
остаётся отдельной операцией. Виртуальные runtime Read/Write/Index/Create/Clone
явно отказывают, чтобы унаследованный base reader не выдал частичную работу
за успешную загрузку Text. Layout unavailable отражён в native/C# API.

## Исправленные ошибки

Предыдущий C# decoder считал строку UTF-16LE, ширину переноса — Single,
требовал font/text/color в фиксированном порядке. Оригинальный reader
PC441C10/416DC0 читает UInt16 byte count и raw bytes; wrap является UInt32.
Factory defaults подтверждены: text/font NULL, colorFFFFFFFF, wrap/alignment0.
Общий inspector принимает omission/unknown/repeated/reordered fields.

Оригинальные side effects Text setter/layout не выполняются при наблюдении
метаданных. Это объявленная граница операции, не их замена. Поля base Renderable
проходят существующий общий reader с настоящим partial Renderable; references
наблюдаются общим reference prefix reader без подставных runtime объектов.
Срез string использует существующий `spStream::ReadString`.

C# сохраняет **все wire-string bytes**, включая trailing/interior NUL и байты
80..FF. Display до первого NUL использует обратимую Latin-1 проекцию; универсальная
кодировка игры этим не установлена. UInt32 wrap больше не преобразуется в float.
Текстовые descriptors и актуальные class dossiers исправлены; исторические
записи локальной базы не переписывались.

TextNode использует прежнюю общую Node/RenderNode metadata-проекцию по прямой
делегации PC4423D0. Удалены отдельный C# parser и выдуманное общее требование
единственного inline-child. **Полный actual TextNode reader/ownership этим не
заявлен**: общий RenderNode metadata adapter сам остаётся границей интеграции.
Строгие ограничения прежнего menu corpus profiler сохранены как явные ошибки
профиля вместо null dereference при неподходящих ссылках.

## Проверки и ограничения

- Native TextInspection и RenderNode suites прошли. Первый fixture использовал
  непредставимые field IDs500/65000; исправлен только fixture на17/250.
  Общий writer корректно отказал; игровая логика не подгонялась под тест.
- Managed: **59 проверок**, synthetic границы плюс **10 TextRenderable и
  10 TextNode одного оригинального menu.smo**. Проверены odd count `AB\0`,
  UInt32FFFFFFFF, omission/repeats/NULL, interior NUL/high bytes, foreign entry,
  malformed diagnostic и TextNode без придуманного обязательного child.
- Viewer FormatTests: **609 assertions**; отсутствующий Samples явно skipped.
  Viewer FormatTests и сам Viewer скомпилированы без warnings/errors.
  Это проверочная сборка, не выпуск или GUI acceptance.
- Fresh full Text reader original/source comparison не заявляется: original
  evidence подтверждает scalar-loop и defaults/setter, а не нашу отсутствующую
  runtime раскладку. FontManager selection дополнительно установлен в отдельной
  bounded пробе; он не подставляется в inspector как успешная runtime среда.

[Исходные PC-факты](tool-text-original-defaults-2026-09-10.md),
[статический разбор](tool-text-inspection-audit-2026-09-09.md),
[manifest](../../research/tools-core-text-inspection-block-2026-09-10.json).
