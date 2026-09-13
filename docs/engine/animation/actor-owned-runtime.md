# PC actor: перенос owned runtime и сквозная проверка

## Восстановленный участок

В `spActor` теперь реализованы `DiscoverNodeForAnalysis`,
`BindDescendantsForAnalysis`, `StartForAnalysis`, `RebindForAnalysis`,
`StopForAnalysis`, `StopAllForAnalysis` и owned-ветка прежнего Tick.
Используются существующие `spAnimationManager`, `spAnimTrack`,
`spNodeController`, `spTransformTrackEval` и `spNode`; второго evaluator/reader нет.

Start сохраняет исходные priority/weight/event semantics, включая active restart
без сброса state weight/sample/stopAfterFade. Request fade durations нужны для
расчёта rates и threshold, но **не копируются** в state14/1C: эти слова остаются
неизвестными. Ранее восстановленные state поля теперь явны в PC ABI struct:
fade10, rates18/20, cookie2C, stopAfterFade3C, status40, slotIndex44; unknown14/1C/38
не получили вымышленных ролей.

Binder сохраняет native порядок: old-animation pointer запоминается **перед
обработкой каждого state**, а не единожды для всего массива. Вставка раннего
state может изменить counter позднего ещё до этого чтения. Pointer-only clear,
cache destination inheritance и exclusive counter asymmetry сохранены.

## Найденная startup-зависимость node

`compare_pc_actor_binding.py` использует настоящий `bbush.san` и два named node:

| Сценарий | Leaf comparisons |
| --- | ---: |
| normal | 915 |
| blend/restart/StopAll | 1299 |
| oneshot | 731 |
| transition | 726 |
| fade_stop | 558 |
| suppressed Stop | 925 |
| **Всего** | **5154** |

Следующий связный фронт: remaining actor public controls и upstream overlapping
input invariant, внешний `spEngineCore` frame helper41CD50/manager owner3C,
затем полная resource/render связь. Event dispatcher/reentry и full FFPS/FAT
transaction/writer остаются неизвестными, importer/exporter не объявляются готовыми.
