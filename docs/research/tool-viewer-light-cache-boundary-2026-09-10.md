# Viewer: источник light cache для одного Icy draw

Read-only mapping, без новых guest/loader runs или source changes. Выбран
pristine `Characters/Icy/Icy.smo`, SHA256
`744EEDD682725C5712789E13598FB478AB7190CBC022CDB98B291E2E900D485C`.

Actual RenderNode ID3/index2 содержит Skin ID4/index3 (`Icy-004`), который
ссылается на mesh ID7/index6 и material ID5/index4. Связь — serialized
RenderNode field0, не вывод из storage parent. В том же файле есть LightData
ID119/index118 (`Ambient01`); существующий whole-graph capture уже показывает
runtime spDXLight `6B3E7BAA`. Own field0 содержит type3, field8 — enabled1.
Это один настоящий ambient light, не fixture и не вывод из имени.

Capture показывает flags `70A00`: hierarchy bit100 отсутствует. Вызовы
`ResourceGraph` и `Scene::sample` сейчас не создают LightManager, не связывают
его с Light/RenderNode и не активируют hierarchy. Они загружают общие ресурсы,
сбрасывают PRS, применяют animation и вызывают virtual world. Поэтому пустой
constructor cache нельзя выдавать за результат actual scene light selection.

Нужные общие операции уже существуют ([CP93](native-pc-skin-selected-light.md)):
`RegisterLightForAnalysis`, `Light/RenderNode::SetSceneLightManagerForAnalysis`,
`RegisterRenderTargetForAnalysis`, затем обычный world update. RenderNode
использует собственные world sphere и controls123/121; Light обновляет живые
targets. `IsEligibleForAnalysis` требует active hierarchy и enabled light.
Для этого ambient light камера/радиус не участвуют в eligibility; billboard
bits также отсутствуют. Общий алгоритм selection переписывать не требуется.

Две возможные тонкие операции следующего блока:

1. На принадлежащем `SparkplugSceneRuntime` графе настроить scene lighting
   context с **явно переданными** light IDs и activeHierarchy input. Pinning
   графа уже есть; manager и borrowed links должны жить до teardown контекста.
   Для этого файла light ID119 известен, а active=true должен быть объявленным
   действием host preview. Это не восстановленный spScene constructor/default.
2. После `Sample` читать selection по
   `occurrence.ContainerObjectIndex → document.Objects[index].Id → spRenderNode`,
   здесь ID3. Возвращать ordinary IDs и отдельный ambient ID из actual cacheF0.
   Один mesh/skin ID не заменяет container identity. Первый getter не требует
   заполнения неизвестных device payload words; `ResolveLightCacheForAnalysis`
   и полный Submit подключаются позже с их явными входами.

Точка managed подключения уже есть: создание `SparkplugSceneRuntime` в
`MainWindow.xaml.cs:848`; source handle и Sample находятся в
`ViewerBridge.cpp:446` и `SparkplugSceneRuntime.cs`. UI refactor для контекста
не нужен. Перед уничтожением manager необходимо снять Light/RenderNode links
и render targets; второй lighting context на тех же Node objects следует
отклонить как host lifetime conflict. Эти операции пока только предложены.

Локальное evidence: `material-preview/light-cache-mapping/icy.json` в каталоге
`local-data/results/tools-core-cycle-20260910-0730/`. Оно сохраняет точные
selected DB rows, source/capture hashes и ограничения. Новые defaults,
NULL/empty light lists, full Scene frame и готовый GPU lighting не заявлены.
