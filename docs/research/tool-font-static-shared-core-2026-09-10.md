# Общий Font reader и запись матриц StaticRenderObject

Первый проверенный блок цикла до 07:30 МСК 10 сентября. UI не перерабатывался,
релиз не упаковывался. Изменены две операции ядра, а не готовность всех ядер.

## Font

Добавлены `spFont` и `spFontSerializer`: PC factory462EC0, reader442660,
glyph assignment442620. Базовый класс — spNamedObject. Высота и atlas pointer
изначально нулевые, 224 glyph entries обнулены; **baseline не инициализируется
игрой** и представлен optional до назначения. Reader сохраняет UInt32 metrics,
байтовые ширины и сырые UV, принимает unknown/repeated fields в порядке чтения.

Один reader обслуживает обычное чтение и явно ограниченное наблюдение atlas
reference. Полный путь использует общий reference resolver и canonical owner;
инспекция сохраняет только metadata ссылки, не создавая подставную TextureData.
Font зарегистрирован в ResourceGraph. Font clone, writer и renderer не входят
в восстановленный срез; неподдержанный clone явно отказывает.

`SmoFontDecoder` стал тонкой обвязкой; самостоятельный C# parser удалён.
Прежние ограничения metrics/UV не были правилами оригинала и больше не мешают
наблюдению raw state. Host проверяет принадлежность entry документу и границы
atlas metadata. Неизвестный baseline доступен потребителю как nullable UInt32.

В review нового кода исправлены foreign-entry acceptance и зависимость текста
ошибки ABI от порядка вычисления C++ аргументов. Оба отказа проверены отдельно.

## StaticRenderObject

Новый ABI вызывает существующий `spStaticRenderObjectSerializer` writer
PC450140. World и inverse подаются независимо; никакой инверсии/нормализации
writer не добавляет. Общая Editing-обвязка подключена в PlacementTransformWriter
и SharedPlacementCloner. Удалён ручной matrix encoder второго потребителя.
Finite guards сохранены как политика редактора; оба диапазона и обе матрицы
проверяются до копирования. Существующая legacy inverse policy не менялась:
её пересмотр отложен пользователем до работы над LVLcreator.

## Проверки

- Native FontSerialization, StaticRenderObject и FullLoader suites прошли.
- Font original/source: **3816 байт** metrics + всех glyphs совпали; вход
  содержит повторный field0, UInt32 extremes и NaN/Inf/-0 UV.
- Новый Static authoring ABI совпал со всеми **5** сохранёнными original outputs;
  EXE и 10 dependencies старого эксперимента перепроверены перед повторным использованием.
- Managed Font: **54 проверки, 10 Font objects одного menu.smo**.
- Managed Static authoring: **50 проверок**, включая независимые/non-affine
  матрицы, ширины заголовков, реальные вызовы placement patch и атомарные отказы.
- Viewer/Importer FormatTests собраны без warnings/errors. Общий запуск Viewer:
  **609 assertions**, каталог `tools/SmoViewer/Samples` отсутствует и его
  проверки явно пропущены. Отдельный выбранный menu.smo проверен выше.

Исходный Font original report сохраняет status=blocked из-за оставшегося
allocation общей инфраструктуры после успешного reader и destructors объектов.
Это не объявлено полным lifetime proof. У первого тестового C++ builder была
ошибка типа указателя; исправлена до успешного запуска. Неудачные build logs
сохранены, успешные результаты не подменяют историю.

Точные результаты, hashes и локальные пути:
[manifest](../../research/tools-core-font-static-block-2026-09-10.json).
Оригинальные Text/Font проверки: [досье](tool-text-original-defaults-2026-09-10.md).

## Следующий срез и ограничения

Старые Text byte-string/UInt32-wrap ошибки ещё не исправлены этим блоком.
Для текущего инструмента нужен metadata inspector; runtime Text setter/layout
имеют дополнительные side effects, не заменяемые успешной заглушкой.
Новая узкая проба восстановила FontManager41E770: borrowed current font выбирает
explicit pointer либо member+28, ARGB записывается в+30, refcounts не меняются.
Инициализация+28 и полный layout остаются за пределами этого блока.
Результаты находятся в локальном `text-font-setup/` рядом с `font-static/`.
