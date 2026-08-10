# Подтверждённые class ID

Источник истины для parser — [`SmoClassRegistry.cs`](../../tools/SmoViewer/SmoViewer.Core/SmoClassRegistry.cs). Таблица фиксирует пары, подтверждённые текущим corpus/executable research; наличие имени ещё не означает, что все поля класса разобраны.

| Hash | Имя класса |
|---:|---|
| `0x763277DB` | `spModel` |
| `0x6160348B` | `spMaterialData` |
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
| `0x3F453DE7` | `spMeshBV` |
| `0x1C0053D6` | `spUVController` |
| `0x234C576B` | `spStdLayer` |
| `0x7F577C6D` | `spMaterialTextureLayer` |
| `0x427C7480` | `spEnvironmentMapLayer` |
| `0x63FEA321` | `spShadowVolume` |
| `0x04680BC1` | `spDXShadowVolume` |
| `0x774E52E3` | `spDXShadowMesh` |

## Правила обновления

Новая запись добавляется после подтверждения как минимум двумя источниками, например:

- регистрационной строкой/таблицей в executable и совпадающим hash в ресурсе;
- несколькими независимыми объектами с согласованной структурой;
- существующим именем класса и воспроизводимым runtime experiment.

Неизвестный hash сохраняется числом в выводе parser. Давать ему «похожее» имя без evidence не следует. Состояние decode полей нужно отслеживать отдельно от подтверждения имени класса.
