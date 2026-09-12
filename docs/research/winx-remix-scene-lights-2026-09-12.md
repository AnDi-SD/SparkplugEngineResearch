# Общий мост сцены и света Winx → Remix

12 сентября 2026. Собственная обвязка PC debug EXE и stock Remix 1.5.2.
После уточнения пользователя Гардиния 2 исключена из дальнейших запусков.
Новые проверки механизмов выполнены на Домино и Алфее. Исходники/данные
игры и поставляемые NVIDIA DLL не изменены; reference checkout остаётся чистым.

## Реализованный контракт

`-SceneLights` передаёт направленные, точечные и прожекторные источники
через штатные CreateLight/DrawLightInstance/DestroyLight. Идентичность задают
сцена и объект, а не слот D3D9, положение на экране, имя уровня или asset hash.
API hash — собственный идентификатор ресурса в пределах процесса.

Источник — `spEngineCore.defaultScene +18`, его `LightManager +34` и полный
невладеющий список lights. Проверяются manager vtable/owner/count, обратные
связи B8/BC, принадлежность каждой записи сцене и конкретный spDXLight vtable.
Лимит 512 записей, чтение собственного процесса через ReadProcessMemory;
никакие указатели игры не сохраняются как владельцы/COM references.

Свет синхронизируется при первом draw основной игровой камеры на основном
render target. View/projection сопоставляются с камерой из EngineCore+1C.
Повторная выборка culling для этого не требуется. Обычный световой режим
**не устанавливает detour игровых инструкций** и работает без DrawTrace.
Отсутствие draw не считается удалением: в конце кадра проверяется настоящее
членство в сцене. При unlink, выключении или снятии hierarchy-active источник
освобождается; смена сцены и Reset снимают прежние ресурсы.

Enabled по +ED и node bit 0x100 взяты из подтверждённого native контракта CP8/CP92.
Положение, направление, цвет и attenuation берутся из уже обновлённого
D3DLIGHT9 payload `spDXLight +F0`. Его игровая формула не переписана в shim.
Для перехода к физическим лампам использован CPU-срез NVIDIA LightUtils
из reference `b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4`, с лицензией и
явной маркировкой изменений. Это политика Remix, не восстановленный Sparkplug.
Общий gain 10, sphere radius 4 игровых единицы, distant diameter 0.0349 радиан;
point/spot используют default least-squares conversion и исходный world space.
Эти параметры не подбираются по уровням. Другой sceneScale пока не подключён.

Неизменные источники повторно рисуются без CreateLight. Изменение параметров
сначала освобождает старый client handle, затем создаёт новый с тем же server
hash: x86 bridge выделяет отдельный mapping на каждый CreateLight. Это
предотвращает накопление mappings при анимации. В рабочем режиме наш слой
управляет `ignoreGameDirectional/Point/SpotLights`, чтобы не удваивать источники.
Неподдержанные параметры геометрического типа оставляют весь этот тип на
legacy-пути; недостоверный registry снимает собственные ресурсы.

## Что проверено

- MSVC x86 `/W4`: сборка DLL без предупреждений.
- `Test-SceneLights.ps1`: **47 проверок**, 9 creates / 9 destroys, 0 оставшихся
  handles. Literal ABI buffers + recording API, без исполнения EXE. Три типа,
  spot angle, point radiance, повторный draw, отсутствие draws, изменение,
  enabled/active, unlink, смена сцены, Reset, malformed registry и отказ API.
  Это проверка обвязки, не GPU и не новые native-class assessments.
- `2026-09-12-scene-lights-alfea`: первый прототип прочитал 27 записей
  (1 directional, 19 point, 7 ambient);20 источников прошли API без серверных
  ошибок. Историческая версия ещё подавала свет из visibility observer.
- `2026-09-12-scene-lights-domino`: итоговая версия, NoDrawTrace, без SceneAudit.
  В Домино создан один источник, при движении ресурс переиспользован.
  Штатный F1 переход на Алфею в **том же процессе**: frame 7627 снят прежний
  источник; frame 7651 созданы 20 новых. Ошибок API/невалидных handles в
  серверном логе нет. Visibility entry в живом процессе остался
  `6a ff 64 a1 00 00 00 00`, что отдельно подтверждает отсутствие detour.
- Сравнение API → legacy → API на Алфее освобождает/восстанавливает 20
  ресурсов, без их накопления. NoDrawTrace действительно не создаёт draws.jsonl.

## Границы результата

Это рабочая передача и время жизни геометрических источников, **не завершённое
освещение игры**. Интерьер Алфеи остаётся очень тёмным и на legacy-пути, и
на новом. Raster-сравнение того же кадра показывает нормальную исходную
картинку. Наличие 20 API lamps не доказывает, что перенесены все материальные
и световые составляющие оригинала; красивый результат не заявляется.

Ambient (type 3) не превращается в сумму глобальных ламп: в игре есть отдельный
ambient cache, в том числе у персонажей. Следующий общий контракт — его
применение совместно с материалами/vertex colors и shader constants.
Источники projectShadow требуют учёта исходных ограничений получателей;
невалидные/nonfinite payloads не нормализуются молча. Spot проверен на стенде,
но не встретился в выбранных живых сценах. Длительная анимация реальных lights,
другой sceneScale и другие EXE/ABI остаются непроверенными.

USD capture при API-материалах по-прежнему имеет ранее зафиксированный сбой;
новый световой мост его не исправляет. В этих тестах capture не запускался.
Полное время жизни геометрии, материалы/деформации и вся регрессия уровней
остаются отдельными незавершёнными пунктами общего плана.

## Запуск

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File research/rtx-remix/Run-WinxRemix.ps1 -Mode RTX -DebugMenu -SceneLights -StartLevel 4 -Windowed
```

Этот экспериментальный общий профиль включает AutoSurfaceRoles и не включает
исторические mesh/texture exceptions. Основной профиль без SceneLights прежний.
Прямой Start-Probe позволяет отдельно включить SceneLights, SceneAudit и LiveConfig.
`winx.keepSceneLights=True/False` в live.conf возвращает legacy/scene путь.
EXE проверяется полным SHA256 launcher и ABI-признаками shim.

[Код](../../research/rtx-remix/winx_scene_lights.h),
[тесты](../../research/rtx-remix/test_scene_lights.cpp),
[manifest](../../research/winx-remix-scene-lights-2026-09-12.json).
