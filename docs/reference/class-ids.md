# Подтверждённые class ID

Источник истины для parser — [`SmoClassRegistry.cs`](../../tools/SmoViewer/SmoViewer.Core/SmoClassRegistry.cs). Таблица фиксирует пары, подтверждённые текущим corpus/executable research; наличие имени ещё не означает, что все поля класса разобраны.

| Hash | Имя класса |
|---:|---|
| `0x763277DB` | `spModel` |
| `0x6160348B` | `spMaterialData` |
| `0x0F507BC8` | `spPS2Material` |
| `0x78EA082B` | `spTextureData` |
| `0x33C34CF0` | `spMeshData` |
| `0x56D67170` | `spStaticRenderObject` |
| `0x695C0F65` | `spNode` |
| `0x603625D0` | `spRenderNode` |
| `0x681F2043` | `spSkin` |
| `0x52E86EFE` | `spTextNode` |
| `0x19A745D7` | `spTextRenderable` |
| `0x4693490A` | `spFont` |
| `0x47A97C0E` | `spCollisionInfo` |
| `0x0A316ECE` | `spCollisionManager` |
| `0x436BFF01` | `spPhysicsManager` |
| `0x21CC76AF` | `spBoundingVolume` |
| `0x3F453DE7` | `spMeshBV` |
| `0x4DA04889` | `spOBBBV` |
| `0x7B4C0876` | `spBoxBV` |
| `0x312FABC0` | `spCapsuleBV` |
| `0x1BCC5322` | `spConvexBV` |
| `0x36432CFF` | `spCollisionMesh` |
| `0x912CC341` | `spPartitionSystem` |
| `0x67672341` | `spPartitionNode` |
| `0x94BBCA2A` | `spPartitionRenderable` |
| `0x21A70829` | `spOctreeNode` |
| `0x61254AB3` | `spZone` |
| `0x6523AC37` | `spZonePortal` |
| `0xABB5AB2C` | `spZonePortalNode` |
| `0x1C0053D6` | `spUVController` |
| `0x234C576B` | `spStdLayer` |
| `0x7F577C6D` | `spMaterialTextureLayer` |
| `0x427C7480` | `spEnvironmentMapLayer` |
| `0x4DED3E44` | `spCubeEnvMapLayer` |
| `0x194613E1` | `spCameraViewLayer` |
| `0x46B61C67` | `spMirrorLayer` |
| `0x075F3EB6` | `spMovieLayer` |
| `0x63FEA321` | `spShadowVolumeManager` |
| `0x04680BC1` | `spDXShadowVolumeManager` |
| `0x774E52E3` | `spDXShadowMeshSerializer` |
| `0x7AC95AEC` | `spFog` |
| `0x5E6402DF` | `spLightData` |
| `0x188A161F` | `spNavigationGraph` |
| `0x74F9013E` | `spNavigationSet` |
| `0x7297173C` | `spMeshNavigationSet` |
| `0x385662AA` | `spNavigationPortal` |
| `0x7362AB22` | `spBSPNode` |
| `0x5AFA1A4F` | `spParticleSystem` |
| `0x56EE563A` | `spAnimation` |
| `0x4C633E85` | `spMaterialColorController` |
| `0x7A7124AF` | `spSkyBox` |
| `0x43D24430` | `spOcclusionVolume` |
| `0x435370B5` | `spLensFlare` |
| `0x16FB0E47` | `spAnimTexController` |
| `0x390946D2` | `spSphereBV` |

## Runtime material/target classes

Эти RTTI-типы подтверждены регистрациями обоих executable и serializer call
graph, но не обязаны появляться отдельными top-level FAT entries в исследованном
SMO-корпусе.

| Hash | Имя класса |
|---:|---|
| `0x694E6975` | `spMaterialTexture` |
| `0x535D1473` | `spMaterialRenderTargetTexture` |
| `0x34EF51B9` | `spMaterialCameraViewTexture` |
| `0x1C3B499A` | `spMaterialCubeMapTexture` |
| `0x6A24474A` | `spMaterialMovieTexture` |
| `0x18DF3845` | `spCamera` |
| `0x24BB4C41` | `spCameraData` |
| `0x41672E34` | `spDXCamera` |
| `0x055A04E0` | `spPS2Camera` |

## Правила обновления

Новая запись добавляется после подтверждения как минимум двумя источниками, например:

- регистрационной строкой/таблицей в executable и совпадающим hash в ресурсе;
- несколькими независимыми объектами с согласованной структурой;
- существующим именем класса и воспроизводимым runtime experiment.

Неизвестный hash сохраняется числом в выводе parser. Давать ему «похожее» имя без evidence не следует. Состояние decode полей нужно отслеживать отдельно от подтверждения имени класса.

Три прежних сокращённых имени shadow-классов исправлены по полной PC registration
table: ID `0x63FEA321`, `0x04680BC1` и `0x774E52E3` принадлежат manager/serializer,
а не `spShadowVolume`, `spDXShadowVolume` и `spDXShadowMesh`. Проверка таблицы
воспроизводится `research/inspect_executable_architecture.py --class-id-table`.

`spNavigationSet` подтверждён регистрацией и serializer-кодом PC/PS2 как
базовый класс `spMeshNavigationSet`; отдельных объектов этого точного class ID в
исследованном корпусе нет.
