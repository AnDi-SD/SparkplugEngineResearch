# PC scene: SkyBox, Projection и LensFlare managers

## Зарегистрированные типы

| Class | ID | Registered base |
| --- | --- | --- |
| spSkyBox | 7A7124AF | spRenderNode603625D0 |
| spSkyBoxManager | 61C23595 | spBaseObject415352A1 |
| spProjectionManager | 24A010DA | spCrossPlatform20A72504 |
| spPCProjectionManager | E10D6FC2 | spProjectionManager24A010DA |
| spLensFlareManager | 782E7D46 | spCrossPlatform20A72504 |
| spPCLensFlareManager | 4838786B | spLensFlareManager782E7D46 |

`spCrossPlatform` в свою очередь наследует `spNamedObject`. Наличие name10
не разрешает перескочить intermediate class в C++-реконструкции. Tables:
SkyManager6EC460(7), PCProjection6F2A00(16), PCLensFlare6F2A5C(13).
Projection base6F34B0 имеет те же runtime methods, но null clone4A1BF0 и
собственный getter/destructor. Concrete PC managers clone только inherited
name через413120; list, enabled и query state не копируются.

## Exact class-ID scene dispatch

- LensFlare435370B5 →scene28 virtual2C/30;
- Projection1CCA7732,32BB2F56,750F73D9,**58DA4026** →scene2C virtual24/28;
- base Projection5CB4145D и похожий, но иной ID не проходят.

## SkyBoxManager: отдельный список и camera-follow

Exact24: Base10, enabled10(default0), borrowed attachmentRoot14(default0),
allocator18(untouched), owned sentinel1C, count20. List nodes12 с payload8
содержат borrowed SkyBox pointers. `48DB00→479EE0` добавляет даже duplicate;
`45A3D0→4CDED0` удаляет **все** совпадения. No intrusive refs у списка.
`48DA20` включает manager и очищает list nodes, сохраняя sentinel. Destructor
48DAA0/deleting48DAE0 очищает list/sentinel/Base, не уничтожает skies/root.
Clone48DCE0 создаёт пустое disabled состояние, не копирует список/root.

Actual SceneManager world с attached native DXCamera и тремя skies подтвердил:
position наследуется от камеры, а SkyBox world49E440 **после** inherited
RenderNode4250F0 копирует свою local orientation40 в world8C. Это сочетание
parent transform и override, не отдельная подстановка camera position в draw.
Inherited world/bounds/children side effects происходят **до** orientation reset;
нельзя переписать их порядок ради более удобной математики.

## SkyBox: не обычный draw и не отключение alpha-sort

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

## Границы описания

Открыты: original source/API names;58DA4026; complete native clone transaction;
LensFlare query-map/occlusion/glare/render; Projection helper4C51B0/virtual38;
Scene45EC70/SceneInit, concrete Partition/Occlusion types и portable scene wiring.
