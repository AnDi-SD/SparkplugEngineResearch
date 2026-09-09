# Occlusion: построение исходящих связей

Блок21 цикла 9 сентября до19:00. В actual `spOcclusionVolume` добавлен
`ConnectOutgoingEdgesForAnalysis`: PC4705A0, независимый PS2 counterpart1CAF90.
Это шестой необходимый метод подготовки topology; full Init/reader по-прежнему
не зарегистрирован в ResourceGraph.

## Исходное поведение

Непустой outgoing list сразу возвращает true. Иначе метод перебирает все edges
в исходном порядке, сопоставляет candidate.start с current.end по componentwise
epsilon0.001, пропускает candidate с совпадающими start/end с тем же epsilon.
Нормализованные направления с dot около1 отклоняются: collinear merge должен
быть сделан ранее. Для border edge проверяется `(direction cross ownNormal)
dot candidateDirection <= 0.001`; internal edge обходит эту проверку.

Допущенный указатель добавляется сразу. Затем рекурсивно обрабатываются
все добавленные указатели. На отказе уже добавленные ссылки сохраняются,
border count/planar/edge membership не меняются. У тупикового ребра true;
это не самостоятельная проверка замкнутости фигуры. Повторный вызов после
частичного отказа может вернуть true из-за уже непустого outgoing list.
Это подтверждённая особенность оригинала, а не исправленная ошибка игры.

## Проверки

Двенадцать новых bounded original-PC запусков: open, turn, concave, collinear,
cycle, reverse, internal, zero, tiny, near, far, partial. В каждом вызваны
actual factory и destructor; вся отслеживаемая память освобождена. Max arena
57168 bytes, прежние100k/2s/call и30s/child сохранены. Geometry/recursion не
подменялись. Каждый случай дополнительно проверяет повторный вызов.

Все12 original/source results и состояния совпали, включая порядок outgoing,
отсутствие удаления edges и partial false→repeat true. Native checks:
OcclusionTopology63/63, FullLoader213/213. Managed/C ABI не менялись и их
проверки не повторялись. Изменённый класс не участвует в графе приложений;
последние managed binaries сохраняют проверенный DLL блока20.

Артефакты: `local-data/results/tools-core-cycle-20260909-1900/occlusion-connectivity/`.
Seal: `research/tools-core-occlusion-connectivity-block-2026-09-09.json`.
Полный Init и protected topology driver не запускались заново. Остались
weld representative order, face/edge construction и planar reverse side.
Используется общая finite vector normalization; универсальная x87 bit identity
не заявлена. Новых успешно загруженных уровней, GPU rendering или релиза нет.
