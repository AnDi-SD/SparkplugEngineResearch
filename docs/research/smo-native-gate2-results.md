# Native Gate 2: transforms и placements

Дата: 29 августа 2026 года. Статус: `Passed`.

## Исправленная ошибка writer

`spStaticRenderObject.InvTransform` не является обычным математическим inverse
при scale. Движок хранит транспонированный 3x3 basis и translation
`-T*A^T`. Старые project-пути `TranslateStaticPlacement`,
`AddReferencePlacementCore` и `SetPlacementTransform` использовали
`Matrix4x4.Invert`; при чистом rotation/translation ошибка была незаметна, а
при uniform/nonuniform scale создавала неверную пару.

Все три пути переведены на
`SmoStaticRenderObjectDecoder.CreateEngineInverseTransform`. Project version 1
получил консервативную миграцию: старое значение исправляется только когда оно
совпадает с математическим inverse и отличается от engine convention.
Произвольная несовпадающая либо неполная пара отклоняется до build.

Также исправлена граница Gate 2: `spModel` собственных PRS-полей не имеет.
Реальные node-владельцы transform — `spNode` и `spRenderNode`; модель получает
размещение от них либо от `spStaticRenderObject`.

## Проверенные операции

| Операция | Результат |
|---|---|
| `spStaticRenderObject` XYZ position/rotation + nonuniform scale | `vase12`, saved/reopen, strict parse и native scene-ready |
| `spRenderNode` XYZ TRS | `vase09-000`, отсутствующие rotation/scale материализованы, native scene-ready |
| вложенный `spNode` XYZ TRS | `numberA` под `lock -> cabinet01 -> Scene Root`; LOCAL→WORLD восстановлен после reopen |
| shared placement | новый `Darch_A02`; `spMeshData` осталось 702, геометрия не скопирована |
| remove added placement | результат побайтно вернулся к исходному SHA-256 |
| remove imported placement | удалены три объекта ветви, все 702 shared meshes сохранены |
| legacy project migration | property journal и reference placement исправлены; случайная пара отвергнута |

Строгий Inspector для всех кандидатов сохранил `signature mismatches=0` и
`decoded meshes=702/702`. Повторные builds до и после добавления миграции дали
одинаковые SHA-256.

## Native-матрицы

Все кейсы использовали contextual route
`Levels\Alfea\Alfea02.smo`, `startLevel=28`, обязательный `SCENE01` и пустую
очередь переходов.

| Матрица | Кейс | Результат | Время |
|---|---|---|---:|
| `mvp-gate2-transform-native-matrix-20260829/run-20260829-141720-458` | pristine before | Passed | 24,934 с |
| та же | `vase12` XYZ rotation/nonuniform scale | Passed | 19,984 с |
| та же | pristine after | Passed | 20,015 с |
| `mvp-gate2-node-transform-native-20260829/run-20260829-142413-855` | `spRenderNode vase09` XYZ TRS | Passed | 21,248 с |
| та же | `spNode numberA` XYZ TRS | Passed | 18,832 с |
| `mvp-gate2-placement-structure-native-20260829/run-20260829-142653-318` | shared Darch reference added | Passed | 19,445 с |
| та же | existing Darch placement removed | Passed | 18,791 с |

Длительный render run находится в
`local-data/validation-results/mvp-gate2-render-evidence-20260829/run-20260829-143436-379`.
Он пережил 45-секундное окно после scene-ready и завершился `Passed` за
63,888 с. Кадр `Alfea02-darch-reference-added.png` показывает реально
отрисованный изменённый Alfea02, а не только успешный возврат loader.

## Контрольные SHA-256

| Файл | SHA-256 |
|---|---|
| pristine `Alfea02.smo` | `1316A81D27254E1B20627433CC8E01327041D560BBEFACB949ADF38A336B79DF` |
| `vase12` XYZ TRS | `D07E510C396C4B2DCB5C37C3D114697A5D0B3B0170B9C1945AC8C09B8FB36797` |
| shared `Darch_A02` added | `6505A88A2B4B3E1E5479204CC1B4B2837849BE98AB0D49D1A2B52D76A916CC42` |
| existing `Darch_A02` removed | `F8FA997667DE37BC162C672C91F48D31067663C5CB1079B32EB18596F22F00C1` |

Release GUI собран без предупреждений. Полный Core-набор на реальной Alfea02
прошёл 1758 assertions; единственное предупреждение ProjectTool/CoreTests —
`NU1900` из-за недоступного внешнего NuGet vulnerability index, не ошибка кода.
