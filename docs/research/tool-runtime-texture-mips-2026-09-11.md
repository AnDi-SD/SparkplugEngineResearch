# Передача runtime mip-уровней в OpenGL

11 сентября 2026, блок 16. Viewer и LVLcreator теперь передают в GPU все
уровни из общего `spDXTexture` CPU shadow. Раньше C ABI и C# передавали только
базовые pixels, после чего OpenGL заново строил уменьшенные изображения.
Это теряло результат уже восстановленного фильтра. Алгоритм движка в данном
блоке не менялся: исправлены транспорт и современный backend.

## Изменение

`spv_graph_texture_mip_info/bgra` возвращают размеры и точные pixels выбранного
уровня. Старый base API вызывает тот же путь для уровня 0. Поддержка остаётся
в прежних границах runtime BGRA/BGRX; у BGRX сохраняется прежняя проекция X в
alpha 255. Размеры и output extent проверяются до записи. Пределы host:
16 уровней, 16 МиБ на уровень и 32 МиБ на всю managed цепочку.

`SmoTexture.MipLevels` хранит общие read-only buffers; base pixels ссылаются
на первый из них. `HasRuntimeMipChain` отличает настоящую runtime цепочку от
отдельного preview, у которого доступны лишь base pixels. `MipLevelCount`
по-прежнему может описывать больше уровней, чем отдал отдельный metadata decoder.
Общая материал-проекция кэширует texture по исходной identity.

GPU загружает каждый имеющийся runtime уровень и ограничивает максимальный
уровень концом цепочки. Для transient/base-only preview остаётся явная backend
генерация. Runtime материал заменяет base preview с той же identity как до,
так и после первой GPU загрузки; старый GPU handle освобождается. Полные
принадлежащие DTO byte arrays передаются без дополнительного `ToArray()`.
Read-only slices при необходимости копируются, и объём копии попадает в отчёт.

## Проверка

Один свежий original run повторяет неизменённый TextureData ID 10 из
`Menus/loading.smo`, ранее исследованный в
[CP108](native-pc-texture-missing-mips.md). 129 571 original инструкция,
0,633 с reader time, 62 480 arena bytes, 49 освобождённых allocations.
Все пять уровней — **1364 байта** — совпали по hash в цепочке original PC
function → C ABI → C# → OpenGL readback. В файле хранится один уровень 16×16,
четыре остальных создаёт фильтр. Original filter здесь не подменяется стендом.

| Выборка | C ABI checks | GPU checks | Уровни |
|---|---:|---:|---:|
| menu.smo, общий atlas | 299 | 42 | 10 |
| loading.smo | 47 | 23 | 5 |
| Icy.smo | 175 | 34 | 9 |

В старом `menu.smo` также хранится **один** уровень, а не десять: остальные
созданы общим runtime. Все 1 398 100 bytes десяти уровней совпали между C ABI,
managed DTO и GPU. Генерация тех же уровней через прежний `GL.GenerateMipmap`
отличается на 20 636 byte components. На loading отличие равно нулю — этот
конкретный материал сам по себе не является чувствительным filter regression.
Upstream CP108 содержит десять случаев, включая неравномерные pixels.

Managed atlas regression: 179 checks. Native TextureSerialization и
LegacyTextureAdapter: 2/2 suites, 1271 checks. Три GPU запуска завершились
без ошибок; menu сохранило 41 Mesh + 10 Text и 51 passes. Late-upgrade check
подтвердил один canonical GPU объект, освобождение старого handle и точный
последний mip. Дополнительные copies при tested uploads: **0 bytes**.
Это уменьшение копирования, а не заявление об измеренном приросте FPS.

Первый managed тест ошибочно ожидал все runtime уровни в исходном файле;
ожидание исправлено с разделением stored/runtime. Первый GPU helper неверно
перечислял non-generic IDictionary; исправлен сам тест. При добавлении поздней
замены устранён конфликт имён PixelFormat в тестовой сборке. Эти промежуточные
результаты не засчитываются как прошедшие проверки.

## Границы

Свежая original-to-GPU проверка покрывает native PC raw branch loading.smo;
menu проверяет транспорт common runtime и использует явно отмеченный legacy
adapter блока 12. Original source selection этого legacy файла остаётся
неизвестным. DXT missing-mip generation, дополнительные runtime formats,
PS2 GPU consumer и полный original frame этим блоком не закрыты. Баллы общей
изученности EXE не изменяются.

[Manifest и привязки исходников](../../research/tools-core-runtime-texture-mips-2026-09-11.json).
Raw результаты сохранены в
`local-data/results/tools-core-cycle-20260911-1900/texture-mips/`; qualified
snapshot содержит выбранные source/artifact hashes, не полную compiler closure.
