# Следующая native граница материала

13 сентября 2026. Read-only карта существующего evidence для продолжения
прямой сцены; новых эмуляций, GPU tests и изменений recovered logic нет.

Первый cohort: нынешний ordinary NativeMesh, один pass **и один обычный
StdLayer/MaterialTexture**, без source overrides/custom shader; начать с
mode2/unlit. Один pass сам по себе не доказывает один layer.

| Input | Подтверждённый native источник | Граница |
|---|---|---|
| material17 words | renderer E4A4..E4E4, installed==selected | CP33 копирует selected+78 **до** controller update; CP32 меняет installed block для modes0/1/6. Чтение selected после update неэквивалентно. |
| diffuseSource | E4E8 raw10/11/12→0/1/2 | Проверять COLORVERTEX. |
| emissiveSource | E4EC raw10/11/12→0/1/2 | Историческое имя ambientSource неверно: actual state148=EMISSIVE, state147=AMBIENT. Не переносить ошибочное имя в Input mapping. |
| ambient | C178 float → квантованный ARGB state139 | Простое копирование float меняет прежний Input; применять общий SubmitLights либо квалифицированный packed cache. State147 пока D3D. |
| RGB/alpha operation | MaterialTexture textureStates[1/2], effective renderer C898 | Общий ApplyTextureStateForAnalysis, затем собственные Decode/DecodeAlpha. |
| stage args / RESULTARG / TFACTOR | В textureStates[9] их нет | Не выводить из pass или renderableColor C194. Узкий следующий producer frontier: states2/3/5/6, 60/141/147. |
| UV selection / flags | raw7/8; stage0 E920/E954 | Общий state mapper с shader override. Начать UV0/disabled transform. |
| UV matrix | MaterialTexture+3C after update; installed stage0 F0F4 | Общий ApplyTextureTransform3ForAnalysis, сохраняющий native3×3→4×4. |
| texture identity / sampler | after AnimTex MaterialTexture+34→DXTexture+3C COM; raw3/4/6 | Общие GetTexture/BindResolvedTexture/state mapping; сверять actual bound COM. Bytes/resource transport пока прежний. |

Точка чтения: scoped native mesh submission, **после** 45F570 UV/AnimTex и
4BC410 texture states/final blend, перед draw4BC290. Текущий D3D draw hook
находится в этой точке; начало4BC670 ещё слишком раннее. Snapshot на draw,
без долговременного material-address cache.

Для mode2 material colors не участвуют: общий путь unlit policy использует
белый diffuse. Подстановка source material colors изменила бы семантику.
После unlit cohort подключать installed material block для lit modes; для
совсем исходного producer пути — snapshot до controller update и существующие
InstallMaterialForAnalysis / ApplyMaterialLightingForAnalysis.

Основания: [CP33](native-pc-material-install.md),
[CP31](native-pc-texture-state-map.md), [CP36](native-pc-material-pass-states.md),
[CP23](native-pc-uv-renderer.md),
[уточнение state148](tools-core-resume-2026-09-11.md).
