# CP110: параллельные профили с отдельными fresh guests

8 сентября 2026. Один и тот же `pc-texture-missing-mips` профиль из10 случаев
прошёл последовательно за29,44 с и с четырьмя процессами за10,74 с:
измеренный выигрыш2,74 раза. Максимальный наблюдавшийся суммарный working set
составил100,16 и340,49 МиБ соответственно. Результаты всех10 children и точные
сравнения сохранены; ускорение не меняет class scores или ограничения guest.

```powershell
python research/native_workbench.py run pc-texture-missing-mips --workers 4 --deadline-utc 2026-09-08T04:00:00Z
```

Повторный замер отдельной утилитой дал29,79→10,44 с (2,85 раза) при
sampled peak100,30→339,39 МиБ; оба прогона завершили10/10 children.
Для воспроизведения в Windows PowerShell с локальной политикой Restricted:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ./research/benchmark_native_profiles.ps1 -Profile pc-texture-missing-mips -DeadlineUtc 2026-09-08T04:00:00Z
```

Параметр ExecutionPolicy действует только на этот процесс PowerShell.
Утилита сохраняет stdout/stderr, profile report fingerprints и summary в
ignored `local-data/results/profile-benchmarks/`. Она наблюдает все процессы
`python` и `Sparkplug*Tests`; память самой PowerShell/Codex в замер не входит.

`native_workbench.py` теперь поддерживает1…4 workers. По умолчанию остаётся1;
для параллельного режима каждый child должен иметь reviewed `parallelSafe:true`.
На этом этапе проверены `pc-texture-native-writer`, `pc-texture-native-source`,
`pc-texture-missing-mips` и `pc-scene-file-profile`: их входы только читаются,
а записываемые диагностические файлы внутри профиля имеют разные имена.
Одновременные отдельные запуски одного профиля требуют отдельного владения
выходными артефактами и этим флагом не разрешаются автоматически.

Runner запускает фиксированные группы до4 независимых Python processes.
Каждый имеет прежний30-секундный timeout, собственную память и новый guest;
состояние между случаями не переносится. При ошибке уже запущенные соседи
завершаются в своих пределах, следующая группа не стартует. До новой группы
проверяется запас31 с перед deadline. JSON results сохраняются в порядке
конфигурации после завершения группы, независимо от порядка её завершения.
Непроверенный профиль, неправильный worker count и deadline отклоняются до
запуска детей. Default sequential semantics сохраняются.

Admission estimate составляет192 МиБ на child плюс64 МиБ для controller;
четыре workers дают832 МиБ в общем бюджете1024 МиБ. Это консервативная оценка
для выбранных профилей, не OS-enforced memory cap. Перед замером было свободно
1708 МиБ из8118 МиБ physical RAM. При параллельной сборке или другом тяжёлом
исследовании необходимо учитывать их память в том же общем бюджете.

Замер опрашивал суммарный working set всех наблюдавшихся `python` и
`SparkplugTextureSerializationTests` каждые200 мс. Это sampled peak, не доказанный
абсолютный максимум между выборками; максимальное число наблюдавшихся процессов
было3 и7. Windows Process wrapper не сохранил exit code после завершения:
он оставлен null, а успех отдельно подтверждён generated profile JSON
(`status=passed`, `exitCode=0`,10 completed children). Измерение относится к
этому профилю и состоянию машины, не ко всему исследованию или GPU.

Проверки runner охватывают реальную одновременность через barrier, остановку
после failed group, сохранение порядка/результатов, deadline до запуска,
отказ без `parallelSafe` и прежние sequential timeout/report semantics.
[CP110 manifest](../../research/native-cycle-checkpoint-2026-09-08-cp110.json)
содержит benchmark, checks и fingerprints.
