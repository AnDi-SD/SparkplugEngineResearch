# Цикл ядер tools до 07:30 МСК, 9 сентября

Поручение: перевести ядра всех tools на одну реконструкцию игровой/движковой
логики; UI вторичен. Недостающие используемые методы исследуются в приоритете.
Если ядра готовы до срока — продолжить последовательное исследование EXE.

Первый блок около 22:55 МСК: CollisionInfo/OBB core, Node ownership и единый
обход отношений Node-derived serializers. Восемь bounded original-PC micro
cases, полный original-PC load C++-written FFPS,55 C++ checks и шесть выбранных
suites прошли. Whole native load освободил31/31 allocations. Полный corpus и
collision queries не заявлены готовыми. Release не упаковывался.

Досье: `docs/research/tools-core-cycle-2026-09-09-0730.md`, детали
`docs/research/native-pc-collision-tools-core-2026-09-08.md`, snapshot
`research/tools-core-collision-block-2026-09-08.json`.

Следующий самостоятельный шаг — использовать существующий native SAN sampler
в SanToVmd, затем продолжить SMO graph. GitHub publication остаётся отдельно
от локальных checkpoint commits; прежнее решение о публикации не изменено.

Второй блок около 23:30 МСК: SanToVmd использует общий native SAN sampler и
spNode world PRS. Python engine-копии удалены, MMD retarget/writer сохранены.
32 tests, 24 VMD / 55 024 keys / 732 original-PC-reference poses, 2 273 C# checks
и шесть C++ suites прошли. Входы и frozen original reference сверены по SHA256,
повторный full-corpus не запускался. Unsupported unused track/NaN дают явный
отказ shared host-loader, без обхода. SMO-reader ещё требует миграции.

Третий блок: общий native resource graph. SanToVmd теперь использует и SMO
reader, и реальные загруженные узлы; старый Python FFPS/Node reader удалён.
37 tests, 24 VMD / 732 original-reference позы, пять character SMO, шесть C++
suites и 2 273 C# checks прошли. MeshBV меню — следующий приоритет.
VS во время работы обновилась и требует reboot; явный VisualStudioPath дал
продолжить проверочные сборки без изменений установки или перезапуска системы.

Четвёртый блок: MeshBV/CollisionMesh/FaceDataContainer в Sparkplug, wxFaceData
в отдельном WinxGameCore без bootstrap. Пять original-PC micro cases,
семь побитных sphere comparisons, C++76 checks и семь suites прошли.
Полный общий loader прочитал меню1 225 objects, tile_bad127 и Bloom121;
регистрация существующего ParticleSystem закрыла дополнительный пробел SFX.
Сфера вынесена из DXMesh в единый helper без изменения алгоритма. Native
query tree не объявлен восстановленным/доступным tools. Досье:
`docs/research/native-pc-mesh-bv-tools-core-2026-09-09.md`.

Пятый блок: SmoDocument использует общий header/FAT/object-header reader,
собственный C# parser удалён. Raw inspection не создаёт неизвестных runtime
классов и сохраняет статус оригинального header validator. 9 262 C# assertions
на одном меню, восемь C++ suites (включая FullLoader213), пять metadata samples
с двумя PS2 payloads и три прежних resource graphs прошли. Добавлен выбор
CheckSuites для адресной сборки/проверки. Подробности:
`docs/research/tool-container-shared-core-2026-09-09.md`.

Шестой блок: удалён самостоятельный C# MeshBV/wxFaceData parser. Прямое
чтение поля и whole-resource loader вызывают один C++ geometry helper;
смещение вершин наблюдается при чтении. Исправлены прежние C# запреты на
native unknown/repeated face fields и независимый count. C++83 checks,
C#9 271/2 946/1 504 на трёх файлах и два whole graphs прошли. Досье:
`docs/research/tool-mesh-bv-shared-core-2026-09-09.md`.

Седьмой блок: общие render mesh buffers и vertex layout вместо C# E0/E1
разборщиков. Пять свежих original PC/ABI сравнений, FullLoader213,
C#9 282/2 957/1 505 на трёх файлах и два graphs прошли. Нормали сохраняют raw
значения, planning header не переопределяет IB/VB. Отмечено старое расхождение
packed combiner cached size; standalone path свежей проверкой совпал, перенос
combined cached member отложен отдельно. Досье:
`docs/research/tool-render-mesh-shared-core-2026-09-09.md`.

Восьмой блок: PS2 mesh prefix/bounds читаются общим serializer. Два original
prefix probes остановлены до allocation, отдельный bounds-only reader прошёл
полностью;3/3,3/3,5/5 owners освобождены. PS2 сопоставление статическое. C ABI
сохранил40/40/24 bytes, C#2 960/9 833/9 285 на трёх меню прошли. DMA остаётся
opaque, header numeric relationships не подгоняются под старые C# ожидания.
Досье: `docs/research/tool-ps2-mesh-metadata-core-2026-09-09.md`.

Девятый блок: raw/PC mip readers общие с исходным loader; TextureTool получает
их через общий C# DTO. Удалён его второй FFPS parser. Выявлена и исправлена
ошибка XRGB alpha в preview/PNG; исходные bytes сохранены. Семь original/ABI
сравнений, C++1007, C#9300/2975, TextureTool4248+40 прошли. Source wrapper,
PS2 metadata и writers пока не перенесены. Досье:
`docs/research/tool-texture-sections-shared-core-2026-09-09.md`.

Десятый блок: typed texture output теперь C++ writer. Оригинал подтвердил
raw NPOT и field1C0/2, embedded MemoryStream потребляется после записи целого
буфера. Четыре writer cases11/11, два source cases7/7 owners освобождены;
ABI bytes точны. C++1007, TextureTool4272, Importer192 прошли,32 edited SMO
побайтно прежние. Досье: `docs/research/tool-texture-shared-writer-2026-09-09.md`.

Одиннадцатый блок: общий encoder заголовков вместо C#.14 original/ABI cases,
terminator, native quirks и явные отказы проверены; C++340, C#9300/2975 прошли.
ID31/real empty/forced extended low ID приостановлены, предложен общий
lossless-адаптер после решения пользователя. Досье:
`docs/research/tool-field-header-shared-writer-2026-09-09.md`.

Двенадцатый блок: reference prefix общий для resolver и C# Node/material.
Short nonnull legacy допущение удалено; старые synthetic fixtures приведены
к доказанному формату. Три original paths, split stream, controlled failed-size
stop, пять ABI refusals, C++266, четыре C# файла и три whole graphs прошли.
Досье: `docs/research/tool-reference-prefix-shared-core-2026-09-09.md`.

Тринадцатый блок: Node scalar body общий с full loader. C# normalization и
угадывание transform удалены, authored/effective flags разделены явно. Пять
original cases, C++226, четыре C# набора и три whole graphs прошли. Host ID
index cache:204.4→1.7 МБ managed allocations за835 nodes, без изменения native
DLL/input/checksum. Досье: `docs/research/tool-node-scalars-shared-core-2026-09-09.md`.

Четырнадцатый блок: world placement получает исходный Node runtime, C# FK и
inverse-bind override удалены. Original nonunit-Q/nonuniform-scale case и1054
whole-graph worlds совпали побитно; C++86, пять Viewer samples, Alfea02 и WPF
build прошли. Неверная обратная запись редактора при parent scale остановлена
финальной native world проверкой; решение нового inverse adapter ожидает
согласования. Досье: `docs/research/tool-node-world-shared-core-2026-09-09.md`.

Пятнадцатый блок: общий MeshData writer вместо трёх C# implementations в
resource replacement, skinned splitting и skeleton carrier. Девять original
field streams совпали; пять guards, C++346, replacement91 и clean-skinned26
прошли. Все11 полных edited outputs побайтно равны baseline. Досье:
`docs/research/tool-mesh-shared-writer-2026-09-09.md`.
