# Оставшиеся общие классы PS2 и контроллеры, 10 сентября 2026

24 имени, уже имевшие PC-оценки, получили независимые PS2 evidence.
PC-оценки сохранены. Выполнены23 original PS2 getters;18 ненулевых factories
разобраны отдельно:16 обычных constructor/table matches и ещё два явных
post-base prefix proof для Actor и TransformTrackEval. Нулевых factories6:
Controller, LensFlareManager, NavigationSet, RenderController, SceneGraphOptimizer,
SubController. У SceneGraphOptimizer точный шаблон constant getter не найден:
остаётся metadata-only5, отсутствие метода не утверждается.

Размеры оригинальных PS2 allocations: Actor84, AnimTrack68, Animation128,
AnimationManager48, CollisionMesh28, ColorFuncEval80, FaceDataContainer32,
FunctionEval56, LightManager36, NodeController24, Scene80, SceneManager36,
SkyBoxManager36, TextureTrack32, Track20, TransFunctionEval432,
TransformTrackEval120, VisibilityManager160. Все factories/read windows
и SHA pristine файлов сохранены в контракте.

## Отдельная проверка конструкции

На Actor автоматический сборщик принял второй вызов factory за собственный
конструктор: это spController0011A110, после которого сама factory00118800
записывает таблицу Actor0048CC00 и собственные поля. Для TransformTrackEval
inline body использует S2, автоматический detector его не распознал.
Оригинальный префикс0011DF40 записывает таблицу0048CE80, slot10=FFFFFFFF,
14/18/1C=0 и останавливается перед original memset004076A8, аргументы this+24,0,12. Ни один из случаев не изменяет игровую логику.

Четыре явных PS2 префикса подтверждают собственные записи Actor,
Controller, SubController и TransformTrackEval, без исполнения SQ/LQ и
без заявления о полном конструкторе. Controller пишет active byte10=1,
links14/18=0 перед входом в регистрацию менеджера; SubController меняет vptr.
Actor устанавливает byte1C=1,byte24=1,word28=0 перед конструированием контейнера30.

## Проверенные методы

`probe_ps2_remaining_controller_contracts.py`:45 случаев за0,774 с.

- Base Clone у Controller0011A160, SubController0011BE60 и
  RenderController0011BDA0 действительно возвращает0. Derived Clone от этого
  не становится нулевым; Actor/Animation/NodeController имеют свои реализации.
- Controller Copy0011A020: префикс после сохранения регистров исполняет actual
  BaseObject Copy00100320 и original completion00104F00. Из собственной области
  копируется только byte10, проверены0/1/255; links14/18 сохраняются у назначения.
  Завершение до original LQ, исходная и целевая записи проверены целиком.
- NodeController slots9/10 читают word14 (001190C0/00116400), включая rawFFFFFFFF.
  Slot11/0011B480 возвращает0 при отсутствующей первой или второй ссылке,
  иначе [[this+10]+10]+9. Это адрес буфера, а не доказательство его срока жизни.
- Track slot7/00120FA0 возвращает float0, slot8/00120FB0 не изменяет запись.
- RenderController slot7/0011BCF0 прибавляет входной float к word20.
  Четыре конечные dyadic пары проверены целиком. Произвольная PS2 точность FPU,
  NaN, переполнение и denormals по R4000 эмулятору не объявляются доказанными.

11 свежих PC исполнений тех же leaf rules за1,481 с: RenderController00423190,
NodeController005FF4C0/005FF3E0. Семантика на этих входах совпала с PS2;
PC NodeController slots9/10 используют один адрес, PS2 — два равнозначных.
PC leaf records явно заёмные и guarded; полного PC lifecycle в этом опыте нет.
PC Track00493060/0048EAA0 отдельно сохранён статически. Existing PC controller
Copy/list evidence находится в `probe_pc_animation_manager.py`; не повторялось
и не выдавалось за новое открытие.

## Intrusive list AnimationManager

PS2 append00118D80/remove00118D10:18 вызовов в составе45 случаев.
Три fresh последовательности добавляют3 элемента и удаляют middle/head/tail,
reverse и forward. Каждый следующий экземпляр эмулятора получает точный полный
результат предыдущего; хост не подправляет связи. Проверяются все1024 байта
явного буфера менеджера/элементов, включая данные удалённого объекта.

| Поле | PC (ранее проверено) | PS2 (новое исполнение) |
|---|---:|---:|
| manager head | +24 | +28 |
| manager tail | +28 | +2C |
| controller next | +14 | +14 |
| controller previous | +18 | +18 |

Append добавляет в tail; новый next должен быть подготовлен вызывающим кодом.
Remove обновляет соседей и endpoints, но собственные links удаляемого объекта
не очищает. Эти опыты используют корректные списки; повторное добавление,
повторное удаление и чужие элементы не проверены. Полный PS2 manager lifecycle,
его name registry и frame callbacks остаются открыты.

## Границы и учёт

Новые PS2 оценки: Actor20, Controller25, SubController20, TransformTrackEval20,
AnimationManager30, NodeController30, RenderController25, Track20;
прочие constructor/getter15, только getter10, SceneGraphOptimizer metadata5.
24 новых platform assessments, PC не меняется; все681 PS2 имени получают
явную оценку, полностью закрытые классы этим блоком не добавляются.

Scouting windows шире таблиц могут захватывать следующий класс: за нулевой
границей они НЕ относятся к именованному владельцу. `scene-optimizer-registration`
в animation-list-scout не имеет подтверждённой атрибуции и исключён из выводов.
`track-eval-factory` и `transform-track-eval-factory` — два имени одного окна
0011DF00, не два класса и не два независимых доказательства.

Контракт `research/ps2-remainder-contracts-2026-09-10.json` сохраняет простые
matches отдельно от candidates; два разрешённых случая приложены как
explicit construction proofs в `controller-contracts-run1.json`.
Manifest содержит SHA источников, контрактов, всех captures и PC/PS2 прогонов.
