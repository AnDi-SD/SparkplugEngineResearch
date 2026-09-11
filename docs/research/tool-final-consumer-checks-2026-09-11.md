# Контроль потребителей общего ядра и исправление SAN progress output

11 сентября 2026, блок 21. После блоков1–20 выполнен небольшой сквозной
контроль ещё пяти потребителей. Viewer и Importer уже проверены на текущем
общем коде в блоках19/20; их неизменённые сценарии повторно не запускались.
Проверка семи потребителей не означает завершение всех операций семи ядер.

| Потребитель | Выбранная операция | Результат |
|---|---|---|
| Exporter | igmenu_opt_pc: GLB и FBX, независимый FBX SDK readback | 207 placements,99 physical meshes,110 variants;6513 checks;207 SDK nodes/99 geometry attributes |
| LVLcreator | Одно повторное размещение: move/undo/redo/export/save/reopen | 423 checks, один изменённый Node53,207 placements сохранены |
| TextureTool | Настоящие Avalonia controls, preview, fixed-size и resize/save/reopen | 16 checks, Bloom_body; два новых SMO |
| SanToVmd | 39 bounded tests и один Icy/xiid → Miku_Hatsune_Ver2 | 51 кадр,51 дорожка,2601 ключ; decoded poses подтверждены |
| WinxHairPatcher | Патч/backup/отказ на копиях pristine EXE | 38 checks, maskA021, original input неизменён |

Всего6990 assertions в четырёх managed операциях плюс39 Python tests и
независимая VMD проверка. Это проверки свойств выбранных файлов, а не6990
файлов или новая доля изученности EXE. Были построены четыре относящихся
к срезу test projects, native rebuild не требовался. Все потребители общего
native core используют SHA256
`EF92713019BD80B172AE79F9EDECCB972AA705EEB7E2D4C866B618AE66C0F073`.

Три managed сборки чистые. TextureTool завершился с прежним несовпадением
версии Avalonia analyzers/SDK (четыре CS9057) и устаревшим Bitmap.Save в
тесте (один CS0618); warnings не скрывались и toolchain не обновлялся.
Два NULL_MATERIAL_RENDER_CONTEXT меню в Exporter сохранены как прежняя
граница default/current renderer state. Полный material export не заявлен.

## Найденная ошибка публичного Python API

Первый full VMD запуск вызвал `convert_files` через импорт модуля в процессе
с Windows-1251 stdout. В отличие от обычного `__main__`, такой caller не
выполняет настройку stdout. VMD записался успешно, но печать `→` выбросила
UnicodeEncodeError внутри conversion try; report пометил готовый файл ошибкой.
Японское имя SAN могло также сломать последующий error print и весь batch.

Общий `_print_status` теперь экранирует только символы, непредставимые в
кодировке консоли. Пути файлов и UTF-8 JSON остаются без изменений; игровой
sampler, retargeting и VMD writer не менялись. Существующий batch regression
расширен strict CP1251 stdout и японским именем исправного SAN рядом с
повреждённым. До исправления он упал, после — подтверждает error/ok по каждому
файлу, сохранность прежнего VMD повреждённого входа и успешное продолжение batch.
Все39 tests прошли; отдельный новый дублирующий набор не добавлялся.

Финальный real Icy/xiid прошёл с **той же CP1251 консолью**, без обхода ошибки
в harness. Независимый reader подтвердил 2601 keys, три позы и десять
сегментов тела; максимум direction error `1.7158475e-6` при границе0.002.
VMD SHA256 `E17F5B646A558172766EE00602C9D5203B3935A885BF1DF2BCD3167966999CD2`
совпал с qualified результатом10 сентября. Conversion+validation0,271s,
peak working set29 069 312 bytes. MMD visual playback не выполнялся.

[Manifest](../../research/tools-core-final-consumers-2026-09-11.json) сохраняет
selected source bindings, исходный ошибочный converter, первый failed report,
финальные outputs/logs и реально загруженные binaries. Raw results:
`local-data/results/tools-core-cycle-20260911-1900/final-consumers/`.
Новая original EXE execution и рост PC/PS2 assessment в этом блоке отсутствуют.
