# Независимая подача целого support: CPU-контракт, 13 сентября 2026

Первый потребитель независимого пакета подготовлен и проверен на CPU: **1136 проверок PASS** после добавления ранней камеры; предыдущий закрытый пакет содержит **840 PASS**. Это собственная обвязка Remix, а не новая восстановленная логика игры. Отчёт фиксирует исходники и проверенный контракт; успешный игровой direct-submit, полнота геометрии и визуальная эквивалентность здесь не заявлены.


Уточнение транспортной границы: в CPU fixture слово «принят» означает возврат recording API. У установленного x86-моста `DrawInstance` возвращает SUCCESS после отправки команды в очередь, без отдельного server ACK. Счётчик instances доказывает только этот клиентский результат, а не принятие renderer API или GPU completion. `SetupCamera` имеет отдельный обязательный ответ сервера; и его SUCCESS не доказывает фактическое потребление камерных данных GPU. Исторические frozen CPU manifests сохранены без переписывания.

## Граница подачи

`ObserveSelection` снимает текущие входы до оригинального prepare. Потребитель работает только с целым support, отсутствующим в фактическом исходном selection игры. Включение переноса отдельно управляется `submitEnabled`/comparison. При квалификации перестаёт добавляться только наш дополнительный элемент visibility vector; исходный selection сохраняется, новый visibility stamp этому support не записывается.

| Условие | Граница реализации |
| --- | --- |
| Объект и фаза | Поддержанные static/partition и обычные inherited RenderNode с известной support identity, текущими scene/root/epoch и witness завершённого native update. Enabled, иерархия, матрицы и callbacks проверяются заново; активные Model/Support scope и несовместимые состояния renderer исключаются. Это не универсальное доказательство долговечности любого игрового указателя. |
| Полная группа | Вектор Models читается целиком, включая NULL, порядок и повторы. Каждый ненулевой Model должен иметь допустимый пакет. Все ресурсы группы готовятся до первого DrawInstance. `[A, NULL, B, A]` даёт вызовы `A, B, A`, а не один экземпляр на уникальный Model. |
| Материал | Обычный DXMaterial, mode 2, один MaterialPassLayer/StdLayer, UV0 без transform, без material-color/UV/animation controllers и статического UV override. Используются общие material/texture mapper и текущий default stage. Неизвестные shader/selectors/callback пути вне cohort. |
| Геометрия и текстура | Поддержанные unweighted native list/strip, полный используемый CPU range, точный общий layout и поколения VB/IB/declaration; подтверждённое совпадение захваченных CPU ranges с transport upload. Текстура — зарегистрированная managed 2D usage 0 A8R8G8B8/X8R8G8B8. CurrentTexture учитывает content generation и открытые записи. |
| Остаточные состояния D3D | Ещё не восстановленные bootstrap states читаются в текущем scope и сравниваются перед подачей. Пакет не выдаёт их за native происхождение. VB/IB/texture берутся по зарегистрированным COM identities, без зависимости от текущего bound texture. |
| Камера и очереди | Нужна принятая WORLD камера этого frame, scene, apply sequence и device epoch, с теми же native матрицами и primary target. General queue и alpha queue пусты; C050=1 допустим для выделенной пустой очереди, поскольку маршрутизирует будущие записи. |

Чтение world использует общие восстановленные PRS helpers; потребитель не вызывает игровые producers и не пишет игровые matrix/material/cache поля. Собственные verified flags относятся к проверке захваченного upload. Шейдеры, skinning, частицы, lit/сложные материалы и полный отказ от D3D visibility extension в блок не входят.

## Камера до первого экземпляра

Ранняя `BeforeFirstSceneInstance` делегирует существующему `AtDraw` только на owner thread, при `drawId == 0`, отсутствии попытки/наблюдения камеры в этом frame и действующих четырёх перехватах Draw: slots 81–84. После последних COM-чтений и получения API повторно проверяется раннее окно. Обычный D3D draw, несовпадение матриц, отсутствующая SetupCamera capability или ошибка SetupCamera не разрешают позднюю повторную попытку. Само отсутствие draw hooks попытку не закрывает: если hooks восстановлены при drawId == 0 и остальные условия сохранены, ранний вход ещё допустим.

Пакет камеры копируется по значению через COM/API. SUCCESS учитывается как принятие только при сохранившихся frame, device epoch и последовательностях pending/selected. Потребитель после Setup заново проверяет собственный scope/revision/selection. API SUCCESS при смене frame или повторном входе в selection не разрешает подачу устаревшей группы.

## Срок жизни и ошибки

Под общим recursive guard используются краткие transport witnesses успешных Create до наблюдаемого final Release/reset; дополнительные COM refs не удерживаются. Перед внешними вызовами expanded vertices, Input и полный Model sequence становятся собственными копиями. После последнего COM-чтения и до/после каждого DrawInstance повторяются pure native/resource проверки, полный selection header и содержимое, Model sequence, камера, texture token и API resource epoch. Изменение NULL-слота или добавление Model также отклоняет группу.

Зависимости material/mesh и исключения выделения памяти обслуживает [общий resource ownership backend](winx-remix-resource-ownership-2026-09-13.md). Ledger выделяется до первого необратимого DrawInstance. Ошибка подготовки сохраняет обычный путь. Частичная ошибка API останавливает оставшиеся вызовы и удерживает omission группы только в текущем scope/frame; затем ledger очищается, а latch запрещает дальнейшую direct-подачу, оставляя fallback.

**Оригинальные D3D draws не подавляются:** `SkipDirectDraw` всегда возвращает false. Неожиданный native DIP для уже перенесённого support учитывается и отключает дальнейшую direct-подачу. Повторный оригинальный selection имеет приоритет и очищает omission ledger. Уже отправленные клиентом instance-команды отменить нельзя: после неожиданного перехода текущий кадр может остаться частичным или получить дубли при последующем оригинальном draw. Контракт не обещает атомарного rollback.

Существующая общая opaque-alpha policy применяется только к собственной API recipe. Native alpha input сохраняется; comparison toggle, отключённая policy и нетривиальный reference проверены отдельно.

## Проверка и происхождение

- `Test-IndependentSubmit.ps1 -Name direct-v4`: 840 PASS, 3093 вызова собственного COM double; [закрытая опись 68 артефактов](../../local-data/rtx-remix/independent-submit-tests/direct-v4/evidence.json).
- `Test-IndependentSubmit.ps1 -Name direct-camera-v2`: 1136 PASS, 3878 вызовов собственного COM double; [новая опись 75 артефактов](../../local-data/rtx-remix/independent-submit-tests/direct-camera-v2/evidence.json). EXE SHA-256 `9A75BA0618D7C9FDC375449041FE9550FF2EA1ACC14F40BBF3E4F939631C3C1C`.
- Финальный snapshot consumer: `602331C69BD279F372B0C78FCAB1F906E823DC7EA2CD946E9DC57719E12E95DE`; camera: `12D3DC8FBCF9BC80911A95A075D7EE604D7BAC01AAAB00FD1AA1AC414B6161EE`. Все шесть основных production файлов совпали со snapshot при закрытии описи.

CPU fixture использует собственные ABI bytes, вручную установленные update/create/owner witnesses, минимальные COM vtables и recording Remix API. Проверены полная квалификация до Draw, порядок/повторы, resource failure, изменение selection/vector/texture/epoch через COM/API, частичная ошибка второго Draw, три отказа выделения ledger и освобождение всех записанных API handles. Ранние camera cases проверяют точные матрицы со signed zero, отсутствие повторного Setup, каждый отсутствующий draw hook, foreign thread, failure/null API, изменение frame/scope, draw reentry и вложенный AtDraw из последнего GetRenderTarget. Вложенный AtDraw даёт ровно один Setup.

История сохранена: direct-v1 не собрался из-за промежуточного изменения DirectUse; direct-v3 — из-за временно дублированного resource epoch; direct-v2 был промежуточным PASS 223. Первый camera fixture direct-camera-v1 собрался, но завершился собственным 30-секундным watchdog: GetTransform ошибочно стоял в тестовом slot 44 вместо 45. Точный путь исключения/модального ожидания не снят; подтверждённый ABI defect исправлен только в тесте, v2 прошёл. Пустые stdout/stderr и исходники неудачного запуска сохранены.

Отдельный ранее выполненный root regression настоящего system D3D9 + recording camera прошёл 165 проверок/12 Setup: [результат](../../local-data/rtx-remix/native-camera-tests/independent-first-window-v1/run.stdout.json). Это проверка общего AtDraw, без bridge/игры; ранние случаи выше доказаны CPU double. Ни эта проверка, ни компиляция не закрывают игровую camera ordering, pixels, полную сцену или весь lifecycle. Предыдущая [наблюдательная граница пакета](winx-remix-independent-packet-checkpoint-2026-09-13.md) остаётся отдельным историческим результатом.
