# Оставшиеся общие классы PS2 и контроллеры, 10 сентября 2026

Размеры оригинальных PS2 allocations: Actor84, AnimTrack68, Animation128, AnimationManager48, CollisionMesh28, ColorFuncEval80, FaceDataContainer32, FunctionEval56, LightManager36, NodeController24, Scene80, SceneManager36, SkyBoxManager36, TextureTrack32, Track20, TransFunctionEval432, TransformTrackEval120, VisibilityManager160.

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

## Intrusive list AnimationManager

PS2 append00118D80/remove00118D10:18 вызовов в составе45 случаев.
Три fresh последовательности добавляют3 элемента и удаляют middle/head/tail,
reverse и forward. Каждый следующий экземпляр эмулятора получает точный полный
результат предыдущего; хост не подправляет связи. Проверяются все1024 байта
явного буфера менеджера/элементов, включая данные удалённого объекта.

| Поле | PS2 (новое исполнение) |
| --- | ---: |
| manager head | +28 |
| manager tail | +2C |
| controller next | +14 |
| controller previous | +18 |

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
