# PC: whole SMO с повторяющимися классами и ID из FAT

CP106, 8 сентября 2026. Неизменённый `SFX/g_crystal.smo` прочитан original
`422B50` и C++: совпали26 объектов,25 связей и7551 state/layer/buffer bytes.
Это включает11 Node,10 DXLight, RenderNode, Model, DXMaterial, Fog и DXMesh.
Результат теперь сопоставляется по ID объекта в файле, а не по одному
представителю каждого runtime класса.

Вход6264 байта, SHA-256
`9ECB8CFEFADD7A30F1411F8235039FB07EA342BA13177B4060DE975158609A0A`.
Whole native load:856194 инструкции /5,114 с;175 curated assertions.
State3300 байт, material layer85, buffers/declaration4166. Имена и class IDs
также точны. Все272 tracked allocations освобождены; COM refs/locks0.
Hook оставляет combiner: его cleanup по-прежнему выполняется явно в fixture.

## Сопоставление объектов

В `spSerializerReadContextForAnalysis` добавлено необязательное host наблюдение
`captureFileObjectIDsForAnalysis`. После успешного whole load оно сохраняет
пары FAT object ID → borrowed object pointer до обычного FAT clear. Наблюдение
по умолчанию выключено, не удерживает дополнительных references и не объявляется
оригинальным native storage. При ошибке загрузки снимок не публикуется.
Проверены два однотипных объекта, повторная загрузка, прежнее освобождение
владельцев и все существующие malformed whole-file cases.

C++ capture использует этот снимок; native observer читает настоящий FAT перед
его очисткой. Capture сохраняет каждый ID, runtime class, state и ссылки на ID.
Для возможных cache aliases выбирается минимальный ID одного pointer; реальные
alias-ветви whole file этим набором отдельно не доказаны. Каждый созданный
объект должен присутствовать в снимке; молчаливого пропуска нет.

При первом сравнении обнаружена ошибка прежней observer-колонки: FAT `+0`
содержит external file ID, одинаковый0 для inline entries; object ID находится
в `+4`. Полный native load тогда уже завершился, ошибка относилась к capture.
После исправления свежие сравнения всех трёх файлов по ID прошли. Старый
режим по runtime class также заново проверен на logo и bloom projectile.
Неизменённые результаты CP103/CP105 не переписаны.

## Пределы и проверка

Профиль file сохранён:1 млн инструкций/8 с на вызов,30 с на процесс,
arena128 КиБ, один native allocation32 КиБ. Для crystal заранее объявлен
предел одного COM buffer8 КиБ вместо стандартного1 КиБ; максимальная занятая
arena89296 байт. Недопустимый COM profile отклоняется до создания guest.
Физический D3D device не используется. Десять RTTI records — проверенная
consumer fixture, не восстановленный CRT startup.

```powershell
python research/probe_pc_scene_file_profile.py g-crystal
python research/probe_pc_scene_file_profile.py logo --file-ids
python research/probe_pc_scene_file_profile.py bloom-projectile --file-ids
```

Source frontend: `SparkplugSceneSerializationTests.exe --asset-file-ids PATH`.
Предел capture32 объекта и input64 КиБ сохраняется. Float bits сравниваются
точно; uninitialized light DC/device cache не входят в snapshot.

[CP106 manifest](../../research/native-cycle-checkpoint-2026-09-08-cp106.json)
содержит пять успешных runs на трёх разных файлах, checks и fingerprints.
Открыты whole textured/skinned/level files, все отказы/ownership варианты,
внешние зависимости и runtime save graph. Class scores не повышены за новый
масштаб композиционной проверки.
