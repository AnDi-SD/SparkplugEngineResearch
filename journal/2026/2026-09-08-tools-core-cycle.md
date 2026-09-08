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
