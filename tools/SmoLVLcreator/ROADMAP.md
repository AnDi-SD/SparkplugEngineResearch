# План SmoLVLcreator после 0.1.0

Минимальный редактор и проектный конвейер завершены. Следующие изменения должны
опираться на реальные отчёты тестирования, а не расширять GUI заранее.

Активная исследовательская граница до первого production-ready запуска сужена до
обязательных native gate: container/writer safety, transforms, rigid/skinned
model import, используемые importer-ом materials/textures и production collision.
Полный список критериев и отложенных областей находится в
[`../../docs/research/smo-lvlcreator-import-mvp-plan.md`](../../docs/research/smo-lvlcreator-import-mvp-plan.md).
Fog, navigation, GUI semantics, специализированные эффекты и PS2 output не
блокируют PC-выпуск, пока остаются read-only и сохраняются без изменений.

## Проверка в игре

- Завершить runtime-матрицу воспроизводимого contextual baseline. Автоматический
  маршрут уже сопоставляет 46 известных level SMO с нативными `startLevel`;
  Release-матрица Gardenia01, Alfea02 и Bloom jeans пройдена 3/3. Отчёт:
  [`../../docs/research/smo-native-gate0-results.md`](../../docs/research/smo-native-gate0-results.md).
- Матрица transforms/placements пройдена: static, `spRenderNode`, вложенный
  `spNode`, shared add/remove и удаление исходного placement дошли до native
  scene-ready. Результаты: [`../../docs/research/smo-native-gate2-results.md`](../../docs/research/smo-native-gate2-results.md).
- Gate 3/5 rigid import пройден на GLB Shrek, OBJ Layla и FBX Flora: normals,
  UV0/UV1, winding, UInt16 split, multi-material, exact BGRA/alpha, shared
  texture ownership и project archive проверены, native matrix — 5/5
  scene-ready. Результаты:
  [`../../docs/research/smo-native-gate35-results.md`](../../docs/research/smo-native-gate35-results.md).
- Gate 6 collision пройден: add/move/link/unlink/regenerate/delete, Group 2,
  registry/inline removal, exact triangle order и непустой `wxFaceData` закрыты;
  native matrix — 6/6, DirectInput gameplay-probe подтвердил остановку стеной
  и camera collision. Результаты:
  [`../../docs/research/smo-native-gate6-results.md`](../../docs/research/smo-native-gate6-results.md).
- Тем же маршрутом прогнать skinned замену персонажей.
- Дополнять профили только для путей, которым кроме известного `startLevel` нужны
  предшествующие скриптовые состояния.
- `SCENE01` реализован: после ненулевого `ResourceLoad` contextual-маршрут может
  потребовать совпадение текущего и активного native state с `startLevel` при
  пустой очереди переходов. Проверка читает контроллер процесса напрямую и уже
  закреплена нативными отчётами трёх baseline-кейсов.

## Формат и сохранение

- Собирать корпус неизвестных SBOO-полей и расширять `SmoSchemaRegistry` только
  после подтверждения их смысла несколькими файлами либо кодом игры.
- `.smolvlproj` открывается только при точном совпадении текущей версии схемы.
  После изменения формата старый проект нужно пересобрать из исходного SMO.
- При необходимости добавить явную команду очистки tombstoned assets проекта.
  Сейчас они сохраняются для Undo, но не попадают в собранный SMO.
- Добавить длительный churn-тест из тысяч циклов add/delete collision и
  add/delete external placement, затем либо доказать ограниченность tombstones,
  либо автоматически компактировать их после выхода из окна Undo/Redo.
- Измерять большие многомодельные проекты и при необходимости заменить
  изолированный serializer worker прямым низкоуровневым
  forest serializer. Полного временного SMO на диске уже нет.

## Редактирование

- Уточнить постоянную семантику связи визуальных объектов и collision shapes.
- Собрать профили качества collision generator по результатам игровых тестов.
- Поддержать skinned level assets только после доказательства связей
  skin/skeleton в runtime.
- Для запечённой геометрии `spPartitionRenderable`, у которой нет static
  placement или уже существующей ссылочной оболочки, добавить отдельную
  операцию «сделать переносимой»: копирование ресурса, rebasing вершин и
  подтверждение результата в игре. До этого такая геометрия намеренно не
  выдаётся за обычный размещаемый предмет.
- Улучшать раскладку команд, горячие клавиши и камеру по наблюдаемым сценариям
  использования.

## Граница модулей

- Парсер, schema/RAW API, renderer, importer и exporter исправляются в общих
  ядрах; SmoLVLcreator не получает локальные копии этих подсистем.
- GUI остаётся клиентом проектного журнала и не пишет произвольные неизвестные
  байты. Общий Core при этом сохраняет возможность читать и точно заменять
  поддерживаемые движком поля.
