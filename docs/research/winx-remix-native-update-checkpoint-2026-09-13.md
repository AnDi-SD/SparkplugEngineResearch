# RTX Remix: установленный world-update witness, 13 сентября 2026

Наблюдатель завершения native world update прошёл **97 x86 CPU-проверок** и
работает в закрытом запуске Алфеи: **720 завершённых observer-проходов менеджера,
717 принятых запросов из 717**. Этот checkpoint подтверждает источник сведений
о фазе обновления мира. Независимая отправка геометрии здесь не объявляется готовой.

Полные хеши, результаты и границы сохранены в
[checkpoint JSON](../../research/winx-remix-native-update-checkpoint-2026-09-13.json).
Подробный ABI и происхождение точек перехвата — в
[контракте world update](winx-remix-world-update-witness-2026-09-13.md).

## Что установлено

Собственный [winx_native_update_source.h](../../research/rtx-remix/winx_native_update_source.h)
оборачивает manager entry `45A7D0` и plain Node world slot `6DC524 → 421420`.
Manager trampoline сохраняет семь целых исходных байт. Обе точки проверяются до
записи; полный startup `scene_audit::VerifiedImage()` выполняется до установки
SceneAudit. Эта startup transaction требует отсутствия параллельных native updates,
а не утверждение об атомарной записи семи байт при параллельном исполнении.

Исходные функции вызываются один раз; возвращается полный EAX. Manager не имеет
boolean-success ABI, Node завершает `ret 4`, его AL тоже не означает успех.
Собственные TLS восстанавливаются через SEH `finally`; native `currentScene` и
dirty bits наблюдатель не исправляет. Токены публикуются после нормального
завершения outer manager и только для фактически наблюдённого root dispatch с
адресом возврата `45A7F2`. Storage ограничен 64 POD-токенами на batch.

Начало следующего update, EndFrame, standalone world producer, смена известных
эпох, reentry и foreign-thread overlap закрывают прежнюю пригодность witness.
Foreign activity учитывается атомарно до увеличения invalidation serial и до
выхода исходного producer, включая SEH. Frame/device metadata остаётся owned
render thread; проверка native данных завершается повторными epoch fences.

SHA256 установленной версии header, совпадающий с замороженным CPU-v3 snapshot:
`30E609E1A51B471CD229C6C6EA2EDE2786D00762B841D4CEA927D87F16785A86`.

## Проверки CPU и история исправлений

[Test-NativeUpdate.ps1](../../research/rtx-remix/Test-NativeUpdate.ps1) собирает
собственные x86 thunks и [fixture](../../research/rtx-remix/test_native_update.cpp).
В `local-data/rtx-remix/native-update-tests/native-update-v3/` сохранены исходный
snapshot, hashes, build log, JSONL и PASS: **97 checks, 31 manager calls,
157 root calls**. Native EXE, эмулятор, D3D device и GPU там не исполняются.

Проверены настоящий entry trampoline и indirect `ret 4` dispatch, full EAX с
нулевым AL, exact caller, повторный вход, SEH и отсутствие повторного producer,
смена scene/root/frame/device/mutation, 64/65 roots, заблокированный foreign
standalone root и byte-identical native состояние с observer и без него.
Тест ограничен watchdog 25 секунд и внешним ожиданием 30 секунд.

Первый compile завершился ошибкой fixture: смешанные `unsigned/LONG` в одном
`auto` declaration; исправлен только тест. CPU-v2 собрался, но Windows не
запустила EXE без повышения прав. В v3 добавлен embedded `asInvoker`, запуск
прошёл с обычными правами. Сохраняются `native-update-v1/build.log` и
`native-update-v2/failure.json`. Независимый review выявил два окна foreign
overlap и неполный final frame/device fence; они исправлены до PASS-v3 и
повторно проверены без блокирующих замечаний.

## Закрытый запуск Алфеи

Источник: `local-data/rtx-remix/runs/play-rtx-20260913-032710-504/`,
`startLevel=27`, запуск `03:27:11 +03:00`, PID **7508**. Анализатор проверил,
что записанный PID завершён до и после чтения. Архивная `d3d9.dll` совпадает с
`build-independent-source-v4/d3d9.dll` и launch hash
`2F5E369FF2A5BF16075647B44B55F8B2A89E794602E6BFBCC555D77D4BE6E8D8`.
Build command ссылается на workspace source; полного отдельного source snapshot
этой DLL нет. Неизменность update header в v4 подтверждена автором запуска и
совпадением текущего header с CPU snapshot; это не подменяет полный build manifest.

| Поле world-update log | Результат |
|---|---:|
| Init `enabled / imageVerified` | true / true |
| Frame records | 727, номера 0–739 |
| Номера без строки | 13 |
| `begun / completed` | 720 / 720 |
| `recorded / published` | 2160 / 2160 |
| Токены на completed batch | 3 |
| `queries / accepted` | 717 / 717 |
| `aborted / overflowed` | 0 / 0 |
| Максимум опубликованных за logged frame | 3 при capacity 64 |
| Plain Node wrapper `rootCalls` | 1 511 300 |
| `sequence`, первое/последнее значение | 3 / 405 082 |
| Размер лога / предел | 120 190 / 16 777 216 байт |

`begun` считает наблюдённые manager entries на owner thread. `sequence` — epoch
инвалидации, растущий также от EndFrame и standalone producer; его разность
нельзя выдавать за число updates. `rootCalls` включает descendant/standalone
plain Node calls. Три опубликованных токена не доказывают три уникальные сцены:
агрегат не записывает scene/SystemRoot addresses. Семь logged frames содержат
ноль опубликованных токенов; отсутствие запросов в них не является отказом API.
`completed` означает прошедшее квалификацию normal manager completion, не native
return success. Активность после последнего Present в эти counters не попадает.

У world producer **SystemRoot = `scene+14`**. У существующего renderer owner
поле **`root = scene+38 → partitionSystem+1D4`**. Это разные связи. В sampled owner
records данного запуска scene `69081476`, partition system `443561300`, partition
root `443785340`; последний адрес не объявляется адресом SystemRoot.

Отдельный independent observer в том же запуске имел **`submit=false`**. Его
717 scans сопровождались 262 675 comparisons: `worldDifferences=0`,
`materialDifferences=262675`, `matched=0`. Это фиксирует ещё не закрытую границу
материалов в v4; последующая диагностика v5 не включена в этот checkpoint.
Работающий phase witness сам по себе не закрывает эту границу, произвольные
derived descendant updates или самостоятельную отправку геометрии.

## Воспроизведение анализа

Новый [analyze_native_update.py](../../research/rtx-remix/analyze_native_update.py)
читает только закрытые логи, ограничивает размер файла/строки, проверяет schema,
порядок counters и стабильность файла, сохраняет первые 64 проблемных frames для
каждого типа отказа. **16 проверок анализатора PASS**, включая malformed tail,
несогласованные counters, disabled init и ограничение failure history;
evidence — `local-data/rtx-remix/native-update-tests/analyzer-v1/`.

```powershell
python -B research/rtx-remix/analyze_native_update.py `
  local-data/rtx-remix/runs/play-rtx-20260913-032710-504 `
  --output <новый-путь.json>
```

Пустые failure histories относятся к полям этого observer, а не ко всем ошибкам
игры, renderer или отдельным причинам отклонения root. Для подготовки отчёта
игра/GPU не запускались, DLL не устанавливались, production код не менялся.
