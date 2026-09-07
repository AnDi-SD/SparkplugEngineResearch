# PC scene: SkyBox, Projection и LensFlare managers

Checkpoint 9, цикл до10:00 МСК 2026-09-06. Продолжение
[RenderNode](native-pc-render-node-runtime.md) и
[LightManager](native-class-sp-light-manager.md), а не новый случайный фронт.
PC SHA-256 `3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
PS2 в этом checkpoint не исполнялся. Здесь восстановлены exact PC ABI и
исполняемые доказательства; **portable классы этих менеджеров пока не добавлены**.
Исходные header/TU/API names не выдаются за найденные.

## Зарегистрированные типы

| Class | ID | Registered base | Factory / exact PC allocation |
|---|---|---|---|
| spSkyBox |7A7124AF|spRenderNode603625D0|49E4C0→13C26A0 /1D4|
| spSkyBoxManager |61C23595|spBaseObject415352A1|48DC80→414600 /24|
| spProjectionManager |24A010DA|spCrossPlatform20A72504|null factory|
| spPCProjectionManager |E10D6FC2|spProjectionManager24A010DA|4C5CE0→13C7C10 /24|
| spLensFlareManager |782E7D46|spCrossPlatform20A72504|null factory|
| spPCLensFlareManager |4838786B|spLensFlareManager782E7D46|4C7240→13D7C70 /38|

`spCrossPlatform` в свою очередь наследует `spNamedObject`. Наличие name10
не разрешает перескочить intermediate class в C++-реконструкции. Tables:
SkyManager6EC460(7), PCProjection6F2A00(16), PCLensFlare6F2A5C(13).
Projection base6F34B0 имеет те же runtime methods, но null clone4A1BF0 и
собственный getter/destructor. Concrete PC managers clone только inherited
name через413120; list, enabled и query state не копируются.

## Exact class-ID scene dispatch

`45A810/45A8C0` сначала получает **exact** ID самого node через virtual10.
SkyBox7A7124AF регистрируется в scene30 и возвращается до обхода renderables.
Для остальных node renderable vectorBC/C0 проверяется по exact record ID:

- LensFlare435370B5 →scene28 virtual2C/30;
- Projection1CCA7732,32BB2F56,750F73D9,**58DA4026** →scene2C virtual24/28;
- base Projection5CB4145D и похожий, но иной ID не проходят.

58DA4026 отсутствует в registered catalog; сохранён как **неизвестный literal
ID**, не назван придуманным классом. Actual scenes/RenderNode/SkyBox и native
Attach/reparent исполнялись. Geometry objects для exact dispatch — явно
borrowed literal fixtures с минимальными registration getter leaves; это не
полный SMO loader/модель. Все manager methods списков при этом оригинальные.

## SkyBoxManager: отдельный список и camera-follow

Exact24: Base10, enabled10(default0), borrowed attachmentRoot14(default0),
allocator18(untouched), owned sentinel1C, count20. List nodes12 с payload8
содержат borrowed SkyBox pointers. `48DB00→479EE0` добавляет даже duplicate;
`45A3D0→4CDED0` удаляет **все** совпадения. No intrusive refs у списка.
`48DA20` включает manager и очищает list nodes, сохраняя sentinel. Destructor
48DAA0/deleting48DAE0 очищает list/sentinel/Base, не уничтожает skies/root.
Clone48DCE0 создаёт пустое disabled состояние, не копирует список/root.

`48DB40` сначала копирует список pointers во временный list snapshot. Затем,
если root14 задан, вызывает native Attach421A60(root,sky) и sky virtual30(1).
Snapshot необходим: Attach снимает/добавляет original manager entries во время
обхода. Проверены перенос в другую сцену и reparent внутри той же сцены.
Temporary nodes/sentinel всегда освобождены в normal path; snapshot refs не
увеличиваются. Не заявляется безопасность удаления borrowed sky внешним callback.

Protected `48DA30` исполнился через VM к body402910: при root14 и count20
снимает первый зарегистрированный sky с root через421720, пока native list
не опустеет. Progress зависит от реальных scene side effects. Foreign-root/
повреждённый list могут нарушить его предпосылки; такие циклы не запускались.

**Корень задаётся игровым кодом**. Ограниченные original blocks
`5DA6CA..5DA727` и `5F9F10..5F9F6A` берут engine1C DefaultCamera,
пишут его в `(engine18.scene30).root14`, вызывают48DB40; первый ставит
camera hierarchy100, второй включает manager10. Whole game class/предыдущий
loader/event transaction этим не объявлен разобранным.

Actual SceneManager world с attached native DXCamera и тремя skies подтвердил:
position наследуется от камеры, а SkyBox world49E440 **после** inherited
RenderNode4250F0 копирует свою local orientation40 в world8C. Это сочетание
parent transform и override, не отдельная подстановка camera position в draw.
Inherited world/bounds/children side effects происходят **до** orientation reset;
нельзя переписать их порядок ради более удобной математики.

## SkyBox: не обычный draw и не отключение alpha-sort

Actual SkyBox49E4C0 выделяет1D4, столько же, сколько RenderNode; primary
6EECEC(14), secondary6EECD4(6) заменяют базовые tables. Native destructor
49E410→425050, copy49E5B0→424980, clone49E540 имеют inherited chain. Полный
nonempty renderable clone ещё не исполнялся в этом checkpoint.

Обычный support draw — **4D74A0, true/no-op**. Отдельный sky pass
`48DA60 → VM →45AD60` вызывает49E5F0 для всех entries, объединяет low-byte
результаты черезAND без short circuit и всегда вызывает renderer454850.
Результат renderer flush игнорируется. Enabled manager проверяет Scene,
не этот dispatcher. Failure одного sky не пропускает остальные.

Protected `49E5F0 →4CC3C0` вызывает423D20(renderable,0) для всех models,
то есть **снимает fog24 с release старых intrusive references**, а не меняет
alpha-sort18. Затем прямо вызывает **базовый** support424B60(this+B4,camera,0),
обходя derived no-op; его результат возвращается. Fog mutation происходит
даже у disabled sky, прежде чем base draw проверит Enabled. Удаление fog не
откатывается. Два borrowed fog refs плюс external ref проверяют decrements
без необходимости выдумывать fog destructor.

## ProjectionManager / PCProjectionManager

Native PC24: CrossPlatform14, enabled14(default0), allocator18 untouched,
owned list sentinel1C/count20. `4CDF60` **ищет duplicate перед append**, в
отличие от SkyBox/Light/LensFlare. `45A3D0` удаляет все matches. Destructor
4CDF20 освобождает list/sentinel и CrossPlatform, не borrowed projections.
Concrete clone4C5D50 копирует name и оставляет enabled/list defaults.

Init45A290 ставит enabled1. `4CDE70` проходит список и вызывает4243D0:
последний при необходимости создаёт helper4C51B0 в projection94, записывает
helper18=projection, вызывает helper virtual0 и сам возвращает1 независимо
от leaf результата. В probe helper уже существовал как явная interface fixture;
его factory/render algorithms не объявлены закрытыми.

PC phases virtual2C/30 указывают на5A7DB0 (true/ret4). Late phase virtual34=
4CDEA0 проходит projection virtual38(camera), AND low bytes без early exit.
Getter38/Setter3C =4C5CA0/4C5CB0 работают с enabled14; сам late loop флаг не
проверяет. Scene45EC70 содержит внешние enable/phase/failure gates.

## LensFlareManager / PCLensFlareManager

Base observed prefix24: CrossPlatform14, enabled14, borrowed intrusive
head18/tail1C/count20; flare previous58/next5C. Append4C5DC0→4CE9D0 без
duplicate поиска, unlink4CEA10→4CE970 обнуляет links и поправляет count.
PC removal4C6880 сначала при наличии query-map entry освобождает четыре
query interfaces, payload/map node, затем вызывает base unlink. **Nonempty
query-map/COM cleanup здесь не исполнялся**, в registration тесте map пуст.

PC exact38 добавляет capability24, query-map allocator28/sentinel2C/count30,
queryCount34. Init4C5E30 вызывает reset4C67B0, затем device virtual1D8 с
`(device,9,null output)` и записывает capability=`result !=8876086A`.
Это **не обычная проверка HRESULT на успех**: другой error80004005 оставляет
capability=true. Init всё равно возвращает1 и включает enabled14. Проверены
три результата explicit D3D interface seam; никакой host COM/GPU call не был
выполнен. Draw4C7210 при capability вызывает4C6D90, затем всегда4CE080;
полное query visibility/glare composition — следующий незакрытый участок.

## Проверки и границы

- `probe_pc_scene_special_registry.py`: **81/81**;
- `probe_pc_sky_box_runtime.py`: **39/39**;
- `probe_pc_projection_flare_managers.py`: **43/43**;
- `inspect_pc_scene_special.py`: **25/25** PC hash/RTTI/tables/CALL/game anchors.

Это результаты отдельных probes; helper modules имеют собственные assertions,
их числа не складываются повторно. Exact PC structs добавлены в SparkplugAbi.h,
full portable manager/SkyBox/Scene class integration всё ещё открыто.
Common100k instructions/2s per call,30s child; tracked original allocations
в normal runs freed. No game/OS/GPU/PS2 execution, no apps/assets writes/release.

Initial fixture corrections:48DA30 имеет no-arg ret, не guessed ret4;
empty sky pass всё равно требует renderer flush boundary;423D20 оказался fog
setter, не alpha-sort; inherited native name-copy разделяет entry и увеличивает
byte refcount, поэтому прежний one-owner release seam был заменён локальным
bounded shared-name seam. EXE bytes и общие лимиты не менялись.

Открыты: original source/API names;58DA4026; complete native clone transaction;
LensFlare query-map/occlusion/glare/render; Projection helper4C51B0/virtual38;
Scene45EC70/SceneInit, concrete Partition/Occlusion types и portable scene wiring.
