# Уточнение Clone слоёв и контекста CloneManager, 10 сентября 2026

Пять ранее остановленных PC классов теперь проходят свои factory, RTTI,
Clone и delete обоих объектов. Общая причина остановки00412C01 была в
исследовательском стенде: original nested Clone очищает глобальную map,
которую упрощённый TimerFixture не инициализировал. Это не отсутствующая
исходная ссылка слоя и не доказательство дефекта игры.

## Подготовка и подтверждённый результат

В pristine startup006D14C0 original constructor0052FD90 получает global00755588,
затем регистрируется teardown006D7DB0 через atexit. В новом bounded стенде
исполняется этот constructor, actual CloneManager factory00412540 (24 байта),
оригинальные pair insertion00412F70, nested Clone00412BE0 и teardown006D7DB0.
Убран прежний pair-recorder seam; методы игры не заменены. atexit и ОС не
запускаются. Эта подготовка уже существовала в старых directed probes:
в частности `probe_pc_render_node_ownership.py` и
`probe_pc_material_texture_ownership.py`; повторное исследование startup не
требовалось. Для MovieLayer дополнительно создаётся actual AnimationManager.

| PC слой | Байты слоя | Созданный payload | Результат Clone payload |
|---|---:|---|---|
| spCameraViewLayer |20| spMaterialCameraViewTexture148 | новый объект148 |
| spMovieLayer |20| spMaterialMovieTexture120 | null |
| spDXCubeEnvMapLayer |20| spMaterialTexture104 | новый объект104 |
| spDXEnvironmentMapLayer |24| spMaterialTexture104 | новый объект104 |
| spDXMirrorLayer |20| spMaterialCubeMapTexture156 | новый объект156 |

Пять fresh гостей/3 процесса —4,125 с,25 собственных операций классов.
Источник после Clone проверен целиком; nested payload ownership и все стадии
удаления записаны. EnvironmentMapLayer ещё в трёх fresh гостях копирует
raw word14=0/FFFFFFFF/80000000; вместе с исходным default4 это четыре случая,
дополнительный пакет2,003 с. Исполняемый файл и восстановленный C++ не менялись.

## Почему MovieLayer получает null payload

MaterialTextureLayer Copy004235E0 сначала удаляет прежний payload назначения,
затем вызывает wrapper00467BC0 → CloneManager00412BE0 для payload источника.
`spMaterialMovieTexture` vtable006EA94C имеет original Clone004A1BF0, который
возвращает0. Layer Copy сохраняет этот0 в destination+10 и всё равно возвращает
true. Поэтому сам новый MovieLayer20 возвращается успешно с null payload.
Это реальное поведение игры; отсутствующий Clone не заменялся нашим кодом.

На PS2 независимо: MovieTexture table0048EA20, Clone001722D0 возвращает0;
MovieLayer Copy00170A50 и CameraViewLayer Copy001703A0 переходят в общий
MaterialTextureLayer Copy001706D0. PS2 source payload передаётся через wrapper
00173290 в CloneManager00105100. Отдельная original continuation00170714
принимает наблюдавшийся null child return, записывает destination+10=0 и
завершается true. Это проверенные компоненты; полного вложенного PS2 Clone
transaction или PS2 teardown этот опыт не доказывает.

PS2 EnvironmentMapLayer Copy00170580 после успешного base Copy переносит
raw word14. Проверены0,4,FFFFFFFF,80000000. PS2 Cube Copy00170460 вызывает
ту же базу и completion ID4DED3E44; Mirror Copy001709A0 прямо переходит в базу.
Всего8 PS2 исполнений компонентов за0,170 с, с проверкой всего1024-байтного
объявленного буфера. Не приписывать платформе PC-результат вместо этих evidence.

## Контекст после удаления слоёв

Movie/CubeEnv/Environment освобождают все собственные и контекстные allocations.
CameraView оставляет лениво созданный spPCRenderTargetManager68 (table006F27E4)
и его три16-байтных allocation. Свежий повтор отдельно вызывает original
manager delete: всё освобождено, remaining=0.

Mirror также создаёт spCameraManager40/table006E718C и
spEngineCore344/table006DC318 с внутренними буферами. В исходном опыте
собственные слои/payload удалены, эти16 context allocations оставлены явно.
Во втором опыте CameraManager успешно удалён; additional EngineCore shutdown
остановился004C38E6 при чтенииCCCCCCCC. На этот момент собственный lifecycle
Mirror уже завершён. EngineCore не доподготавливался выдуманными значениями;
эта отдельная задача shutdown остаётся открытой. Повторный отчёт со статусом
blocked не отменяет25 завершённых собственных операций пяти слоёв.

## Исправления прежних формулировок

Документ `native-engine-core-remainder-2026-09-10.md`, строка с MovieLayer,
ошибочно сообщал36 PC/20 PS2. Верно20/20. PC payload имеет120 байт и ссылки
на отдельные36- и32-байтные allocations. Старый machine contract и raw initial
уже содержали правильные20, поэтому оценки/знаменатели задним числом не менялись.
Прежние imported evidence сохранены; этот документ уточняет выводы.

Формулировка «отсутствующая исходная ссылка» у00412C01 в предыдущем платформенном
досье заменяется установленной причиной: нет original static clone-map startup
в том профиле стенда. Другие ошибки по адресам и чужие контексты из этого не
считаются автоматически решёнными.

Scouting labels `render-target-manager-factory` и `engine-core-factory` в
context-startup не являются правильными входами соответствующих factories и
исключены из атрибуции. Identity retained objects установлена независимо по
original vtable, exact getter и registration record; не по этим подписям окна.

## Учёт

PC CameraView25, Movie30, DXCube25, DXEnvironment35, DXMirror25 (прежде15).
PS2 CameraView20, Movie25, PS2Cube20, PS2Environment25, PS2Mirror20 (прежде15).
Ранее более высокие оценки общих Material* классов сохранены. Никакой класс
этим блоком не объявлен полностью закрытым. Контракт хранит class lifetime
и additional-context shutdown раздельно, SHA всех версий probe и raw evidence.
