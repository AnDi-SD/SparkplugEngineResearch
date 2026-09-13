# RTX Remix: оставшиеся bootstrap inputs ordinary draw, 13 сентября 2026

Из рассматриваемых полей **stage1 COLOROP имеет обязательный native producer и не является инвариантом**. Шесть перечисленных render states, sampler0 sRGB, stage0 arguments/RESULTARG и TFACTOR наследуются из текущего D3D context: подтверждённый непрерывный ordinary-путь их не записывает. Это позволяет использовать квалифицированный context snapshot, но не превращает его в состояние, принадлежащее каждому material, и не доказывает сохранность через произвольные промежуточные draws.

Граница: exact supported Support Prepare → exact Model Pre/Render → ordinary DXMesh → один exact pass/StdLayer/MaterialTexture, finalBlend0, native/effective lighting mode2, без Model callbacks, material/texture controllers, static UV, skinning, custom/reentrant leaves. Исследование read-only; игра, GPU, новые native probes и сборки не запускались. [JSON evidence](../../research/winx-remix-independent-draw-bootstrap-contract-2026-09-13.json) содержит ссылки/хеши существующего исследования и 11 bounded native windows с 34 instruction anchors. Все окна совпали между pristine `3F022480…` и debug `C27EA9DB…`; это static verification, не новые behavioural checks.

## Локальная сохранность и собственные producers

| Вход | Источник и сохранность на указанном пути |
|---|---|
| RS15 `ALPHATESTENABLE` | Текущий D3D context. Startup запрашивает1; material mapper его не меняет. Число1 не объявляется универсальной runtime-константой. |
| RS27 `ALPHABLENDENABLE` | Текущий D3D context. Startup запрашивает1; finalBlend0 задаёт только SRC/DEST factors `(ONE,ZERO)`, не enable. |
| RS171 `BLENDOP` | Текущий D3D context. На reference device после Create наблюдался ADD1; ordinary material/pass не является producer этого поля. |
| RS168 `COLORWRITEENABLE` | Текущий D3D context; числовой игровой default не выводится. |
| RS206 `SEPARATEALPHABLENDENABLE` | Текущий D3D context; первый cohort требует фактически прочитанный0. |
| RS52 `STENCILENABLE` | Текущий D3D context; первый cohort требует0. Shadow phase пишет этот state вне ordinary-пути. |
| Sampler0 state11 `SRGBTEXTURE` | Наследуется; CP31 пишет только sampler1/2/4/5/6/7. Первый cohort требует фактически прочитанный0. |
| Stage0 states2/3/5/6, `COLORARG1/2`, `ALPHAARG1/2` | Наследуются; CP31 пишет stage1/4/11/24. Сохранить exact words TEXTURE / CURRENT-or-DIFFUSE, без modifiers. Их эквивалентность ограничена stage0 unlit FFP. |
| Stage0 state28 `RESULTARG` | Наследуется; требовать CURRENT. Из raw9 material texture words это поле не получается. |
| RS60 `TEXTUREFACTOR` | Наследуется. ModelPre записывает Model+28 в renderer+C194, **не** в D3D RS60. При текущих поддержанных TEXTURE/CURRENT-or-DIFFUSE операциях TFACTOR не выбран аргументом. |
| Stage1 state1 `COLOROP` | **Производится заново** CP36 из default material и stage1 selector. Однослойность активного pass не доказывает DISABLE. |

Основание — точные writer sets общих [CP30](native-pc-material-state-map.md), [CP31](native-pc-texture-state-map.md), [CP32](native-pc-material-lighting.md), [CP33](native-pc-material-install.md), [CP36](native-pc-material-pass-states.md), их [полное соединение CP42](native-pc-renderer-submit.md) и [no-weight draw CP40](native-pc-renderer-fixed-draw.md). Shared mapper material пишет RS7/8/9/14/19/20/22/23/24/25 и lighting29/137/145/148; указанная bootstrap-группа в этот набор не входит.

Support Prepare меняет world/inverse и renderer caches. [ModelPre](native-pc-renderer-protocol.md) без callbacks делает native selection/override и [Fog](native-pc-renderer-fog.md), который пишет только собственные states28/34–38. Controller-free active texture update не эмитирует UV matrix и не потребляет animation clock. Exact DXTexture binding передаёт texture/palette; no-weight unlit draw использует подтверждённую shader/buffer/constant цепочку. Эта композиция ограничивает вывод известными leaves: поведение произвольных callbacks, custom shaders, object destructors с чужой reentry и всей очереди renderer здесь не доказывалось.

Общие реализации: [material mapper](../../Sparkplug/Code/SparkplugDX/spPCMaterialStateMapping.h), [texture mapper](../../Sparkplug/Code/SparkplugDX/spPCTextureStateMapping.h), `spDXRenderer::ApplyPassTextureStatesForAnalysis` и `ApplyPassBlendForAnalysis` в [spDXRenderer.cpp](../../Sparkplug/Code/SparkplugDX/spDXRenderer.cpp). Вторые копии таблиц не нужны.

## Минимальный guard stage1 DISABLE

Actual `4BBBA0` после active layer заполняет unused stages. `4BBC73` читает renderer+C9C0, `4BBC79` — default material+4C. Для каждого stage>0 `4BBCA8..4BBCB0` выбирает **layer slot1**, затем `4BBCB4..4BBCEC` копирует девять raw words из nested holder. Для stage1/state1 путь ровно такой:

```text
defaultMaterial = Word(renderer + C9C0)
defaultPass     = Word(defaultMaterial + 4C)
defaultLayer1   = Word(defaultPass + 1C)
holder          = Word(defaultLayer1 + 10)
rawColorOp      = Word(holder + 14)
selector        = Word(renderer + C770)  // C748 + 4*(9*1 + 1)
```

Для первого direct cohort:

1. Проверить readable pointers и известные exact layouts: DXMaterial primary6EF264/interface+14=6EF238, MaterialPassLayer6E7388, StdLayer6E7700, MaterialTexture6E8440. Это консервативная квалификация нашего reader.
2. Требовать `selector==0`. Native `4BBD50..4BBD61` использует texture selector независимо от C1C4. Ненулевой selector выбирает другое desired72-word source; его общий перенос пока не выполнен.
3. Применить существующий `ApplyPCTextureStateForAnalysis(stage=1,index=1,rawColorOp)` к локальному cache/recording sink. Требовать stage COLOROP=`1/DISABLE`. В известном диапазоне raw0..15 этому соответствует только **raw0**. Не брать D3D value1 как raw1: raw1 отображается в SELECTARG1.
4. Сохранить также текущий actual D3D stage1 DISABLE guard. Native raw cache `renderer+C8C0` может подавить вызов COM; lower HRESULT игнорируется. Desired raw0 сам по себе не исправляет рассогласование device/cache после ошибки и не доказывает actual DISABLE.
5. Перечитать цепочку, raw и selector перед потреблением пакета вместе с existing scene/owner/phase fence. Владение объектами или их неизменность на будущих кадрах reader не получает.

**Особенности counts/null важны.** Original не проверяет default material.passCount и default pass.layerCount: даже при declared layerCount1 он всё равно читает slot+1C. Требование count≥2 допустимо как более строгий host guard, но его нельзя описывать как native ветку. В native observed pass layout этот slot физически присутствует; валидность конкретного pointer всё равно обязательна.

Null/unreadable default material, pass, layer1 или holder не имеет ветки «взять layer0/активный material/выключить stage». Direct export должен отказать, оставив существующий D3D path; reader не чинит некорректный native graph. **Holder.fallbackTexture+34 может быть NULL:** для unused stage игра копирует только raw words, не вызывает его GetTexture/Update и задаёт desired bound texture NULL. Проверка `CurrentTextureMissing` к этой raw-only роли не применяется; resource bytes и texture-lifetime token для неё не требуются.

PC factory/default graph по-прежнему не выдумывается: [старый fallback audit](tool-renderer-fallback-material-boundary-2026-09-10.md) явно оставляет producer C9C0 незакрытым. CP36/42 использовали объявленный fixture. Свежий read текущей цепочки решает consumer qualification, но не восстанавливает неизвестный constructor и не переносит PS2 defaults в PC.

## Alpha и граница scene-wide snapshot

`ScopedOpaqueAlphaTest` — собственная policy адаптера: временно меняет только **RS25 GREATEREQUAL7 → ALWAYS8 → GREATEREQUAL7**. Она не выключает RS15. Для native packet брать enabled из квалифицированного context, а function/ref из shared material mapper; comparison provenance восстанавливает исходный RS25. Если direct exporter должен сохранять нынешнюю normalization policy, он применяет её отдельно и явно — не маскирует её изменением native enabled.

Чтение шести RS/arguments до `Prepare` даёт действительный D3D context этой точки. Локальный ordinary draw этот набор сохраняет. Однако между SceneSelection и конкретным будущим Model могут пройти другие supports, queues или эффекты; единственное сравнение либо совпадение на видимых objects не доказывает их общую безвредность для offscreen packet. Direct путь должен потреблять явно выбранный текущий context под своими frame/scene/device границами и не утверждать, что это восстановленный per-material producer всех переключателей.

## Камера в SceneSelection

Native `45EC86` вызывает camera v3C/Apply; на успешной partition-ветке `45ECA7` затем вызывает Sky48DA60. [CP14](native-pc-scene-render-runtime.md) подтверждает порядок **camera Apply → Sky → Visibility/selection → supports**. Значит selection действительно позднее apply, но **не обязательно раньше первого main-classified draw**: Sky уже мог рисовать.

`native_camera_source::Prepare` не является чистым probe: после D3D reads он может установить selected/attempted flags, закрыть окно и отправить SetupCamera. Перенос туда безусловного first-camera вызова доказательств не имеет. При first-update-wins поздний API SUCCESS не подтверждает принятую в этом frame камеру.

Согласованный минимальный путь — использовать уже квалифицированную existing first-draw камерой запись с `selected.valid`, совпадающими frame/scene/main/device epoch и `submittedFrame==frameId`. Если она была принята на Sky до observer, её можно проверить как текущую; если её ещё нет, direct fallback, без создания второго «успешного» late camera submit. Эта проверка использует сделанную работу и не требует нового camera research или повторного native Apply.
