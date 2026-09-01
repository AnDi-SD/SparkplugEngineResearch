# Native Gate 0: воспроизводимый scene-ready baseline

Дата: 29 августа 2026 года. Статус: `Passed`.

## Что именно подтверждено

Contextual validator больше не считает ненулевой результат `ResourceLoad`
готовой сценой. После принятия целевого SMO он читает принадлежащий процессу
`wxGameFlowController` и требует одновременно:

- текущий state `controller+0x1B0` равен запрошенному `startLevel`;
- ожидающий state `controller+0x1B8` равен нулю;
- индекс вершины стека `controller+0x1AC` допустим;
- объект на вершине `controller+0x15C+index*4` существует, а его ID по `+0x10`
  также равен `startLevel`.

Только после этого создаётся `SCENE01` и начинается survival window. Проверка не
зависит от `GameStateLog.txt`: игра буферизует этот файл, поэтому во время
процесса он непригоден как синхронный checkpoint.

Ранее предполагавшаяся универсальная последовательность `74 → 1` неверна.
Она относится к обычному Gardenia01-проходу. При прямом `startLevel=28`
Alfea02 остаётся в native state 28. Первый Alfea02 snapshot после возврата SMO
показал `current=28, pending=28, active=28`; через 4,36 с переход завершился как
`current=28, pending=0, active=28`, после чего был принят `SCENE01`.

## Release-матрица

Воспроизводимый манифест:
`tools/SmoViewer/SmoNativeValidator.Cli/manifests/mvp-gate0-scene-ready.json`.

Итоговый запуск:
`local-data/validation-results/mvp-gate0-release-matrix-20260829/run-20260829-140014-186`.

| Кейс | Trigger | State | Дополнительное доказательство | Результат | Время |
|---|---|---:|---|---|---:|
| Alfea02 | `Levels\Alfea\Alfea02.smo` | 28 | FFPS `0x26`, non-null resource, `SCENE01` | Passed | 56,478 с |
| Gardenia01 | `Levels\Gardenia\Gardenia01.smo` | 1 | FFPS `0x26`, non-null resource, `SCENE01` | Passed | 23,621 с |
| Bloom jeans | `Characters\Bloom\bloom_jeans.smo` | 2 | FFPS `0x26`, `CP08`, `SCENE01` | Passed | 21,813 с |

Холодный запуск заметно плавает, поэтому production timeout остаётся 120 с;
наблюдавшийся диапазон полного baseline-кейса — 21,8–56,5 с.

Отдельный Alfea02 render run с 60-секундным окном завершился `Passed` за
77,390 с:
`local-data/validation-results/mvp-gate0-release-alfea02-render-evidence-long-20260829/run-20260829-140418-588`.
Сохранённый `Alfea02-scene-ready.png` визуально показывает отрисованный интерьер
Alfea после `SCENE01`. Gameplay interaction остаётся отдельной проверкой Gate 7.

## Контрольные SHA-256

| Файл | SHA-256 |
|---|---|
| `WinxClub.exe` | `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F` |
| `Levels/Alfea/Alfea02.smo` | `1316A81D27254E1B20627433CC8E01327041D560BBEFACB949ADF38A336B79DF` |
| `Levels/Gardenia/Gardenia01.smo` | `83D8CAC427E0269DC3E74AEC9579FB5A0FA19442BFB02569E4DFF24D1DEB267E` |
| `Characters/Bloom/bloom_jeans.smo` | `17184190AAB5C45CFE7EB594B6B166A343CD688D99C236FEA3508CA5809B28CA` |

Release-сборки validator CLI и SmoLVLcreator GUI завершены без предупреждений;
набор `SmoNativeValidator.Tests` проходит 299 assertions. Для будущих
release-кандидатов этот baseline выполняется как часть
`pristine → mutation → pristine`; на Gate 0 сама mutation ещё не создаётся.
