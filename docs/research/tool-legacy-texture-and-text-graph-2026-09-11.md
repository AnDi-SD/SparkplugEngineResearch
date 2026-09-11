# Legacy texture policy и Text graph для инструментов, 11 сентября 2026

Блок12 цикла до19:00. Общий host loader теперь загружает старые сохранённые
texture pixels, а C# получает настоящие `spTextRenderable`, `spTextNode` и
`spFont` из общего native graph. Это продолжение
[восстановления Text CPU runtime](tool-text-runtime-2026-09-11.md).
Игровые алгоритмы в этом блоке не менялись; адаптер является явно названной
политикой инструментов, разрешённой пользователем11 сентября.

## Граница совместимости

`LegacyTextureAdapter` устанавливается только новым `spv_graph_load_for_tools`.
Оба прежних graph loader остаются strict. Для common TextureData адаптер
распознаёт ограниченную форму: единственный непустой field0 ровно покрывает
секцию перед последним terminator. Временная копия получает префикс
`22 00 00` — SourceNone envelope; затем **тот же** `spTextureDataSerializer`
читает исходные cross pixels и создаёт texture/mips. Новый pixel decoder
не добавлен. Остальные формы направляются прежнему reader.

Лимит входа16MiB, временная память size+3; внутренние лимиты оригинального
analysis reader сохраняются. Предельный размер около16MiB отдельно не
проверялся. Исходный SMO, FAT и его физические reference observations не
переписываются. Отчёт содержит каждый адаптированный ID. `UsesHostCompatibility`
и `LEGACY_TEXTURE_COMPATIBILITY` проходят через immutable/live graph в общий
scene diagnostic. Прежний строгий skin writer этим режимом не заменён.

Такой envelope подтверждает загрузку сохранённых pixels общей реализацией,
**не** исходную source-selection ветку игры для старого формата. Эта ветка
остаётся открытой. C# legacy metadata classifier пока тоже существует;
устранение всех прежних классификаторов не заявлено.

## Text/Font DTO

Добавлены атомарные C ABI views для текста, текущего Font, glyph table,
runtime bounds и cached Text у TextNode. C# не измеряет и не раскладывает
строку заново. Неинициализированные bounds передаются с presence mask.
Runtime text — байты до первого NUL; raw wire inspector сохраняет свою
отдельную роль. Fonts и atlas textures имеют общую идентичность DTO,
канонические IDs принадлежат загруженному graph.

## Проверки

Raw: `local-data/results/tools-core-cycle-20260911-1900/text-legacy/`.
Манифест: `research/tools-core-legacy-texture-text-graph-2026-09-11.json`.

| Контроль | Результат |
|---|---|
| Native adapter suite | 52 checks; strict/wrapped/direct, cursor/canaries, malformed формы |
| `Menus/menu.smo`, C ABI | 372 checks;238 объектов,63 Nodes,0,248s; адаптирован ID30 |
| `SFX/book.smo`, C ABI | 28 checks;15 объектов,5 Nodes,0,071s; адаптирован ID12 |
| `Characters/Icy/Icy.smo`, C ABI | 131 checks;119 объектов,88 Nodes,0,050s; адаптер не понадобился |
| Menu managed final | 124 checks,0,399s,45,8MiB peak; десять Text/Font/TextNode цепочек |
| Book / Icy managed | По10 checks;0,160/0,171s; input unchanged |
| Book OpenGL | 8 checks;2 placements/2 passes,1 texture,28010 non-background pixels |

Все десять Text содержат `0`, ширина16; Font height32/baseline6. Один atlas
512×512/10 runtime mips общий для Fonts. Его base BGRA32 SHA256:
`9DAD1225A6ABBE7C13C46053FFB4ABA80D556B626CBE51E6C859101831C9D7CF`.
Stored base pixels и runtime pixels совпадают. Проверены неправильные типы,
ёмкости и flag2 с выходными canaries; полная reference trace доступна.

Book GPU: RTX3070,192×192; live frames0,23–0,32ms, peak176,1MiB.
PNG просмотрен: геометрия и знак видимы. Это современный preview, не сравнение
с кадром оригинальной игры. Compatibility diagnostic остаётся видимым.
Native, FormatTests и GuiTests контрольные сборки прошли без ошибок.
Повтор всех43 Viewer poses не запускался: связанный контроль ограничен тремя
graph loads, CPU Text projection и реальным GPU consumer.

## Найденные ошибки и следующий шаг

Начальная synthetic native fixture использовала uint8_t вместо std::byte
и ожидала неизменную высоту1; оригинальная Texture нормализует её до2.
Исправлен тест (2×2), production reader не менялся.

GPU menu check остановился до рисования: ни один из41 Mesh не подготовлен
(`Host mesh view index is outside its vertex buffer`). Managed final report
сохраняет все41 ошибки и10 ещё неподдержанных Text render occurrences.
Следующий отдельный блок — проверить фактические index buffers по оригиналу.
Полная видимость меню и GPU Text **не готовы**, несмотря на рабочий graph.
Text writer/clone/default-font startup, PS2 layout и полный игровой frame
этим блоком не закрываются. Общий EXE score не увеличивался.
