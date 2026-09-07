# PC spSceneManager: borrowed list и world caller (CP71)

2026-09-07; pristine EXE SHA-256
`3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F`.
Продолжает прежнее [native scene/world](native-pc-scene-world.md).

Перенесён отдельный source класс `spSceneManager`, ID67419388/base415352A1,
с прежними exact PC extent24/vtable6E7154. Фабрика45ADF0, clone45AE50,
deleting dtor45ADD0 и world traversal45A7D0 выполнены в original guest.
Source/header path и имена публичных analytical API выведены, не найдены
в debug path. Host std::list и pointers не считаются PC ABI.

Manager владеет list entries/sentinel, заимствует сцены. Constructor публикует
singleton75DB90; destructor обнуляет его без проверки текущего экземпляра.
Clone создаёт пустой manager, публикует его, регистрирует пару и не переносит
list/currentScene. Source воспроизводит это, включая очистку singleton при
удалении старого manager после создания clone.

45A7D0 идёт по списку, публикует scene20, вызывает scene14 root virtual30(0),
затем читает next. Scene24/25 и Node Enabled200 не фильтруют обход; return
root игнорируется. Current обнуляется также при пустом списке. Новые callback
сценарии подтвердили post-call next: append посещается в том же проходе,
remove-next пропускается; false-root не обрывает остальные вызовы.

Source `SceneForAnalysis` — явное представление только borrowed root14.
Это не реализация `spScene` constructor, его четырёх managers, typed
registrations или renderer. Register/Unregister API задают внешний список;
native fixture также явно готовит views/entries. В world-three работают
настоящие Node bodies. В mutation/false случаях root virtual — объявленный
внешний callback; неизвестные engine helpers не подменяются.

Host guards: NULL root, duplicate, 4096 entries/visits, повторный Update и
удаление текущего элемента. False от source Node safety check агрегируется
без short-circuit; исходный native helper void не определяет такой результат.
Caller обязан сохранять borrowed views/root до unregister. Callback удаления
manager/current scene и произвольные typed reparent остаются открыты.

**6 exact captures,50 native assertions,42 source assertions.** Empty,
world-three,append,remove-next,false-root,clone. Полный trace текущей сцены/
root/argument, raw world position/scale, remaining list и clone lifecycle.
World update максимум1898 instructions, arena≤1248 bytes; all tracked owners
freed. Caps прежние100k/2s/call,30s/child,64KiB/32KiB allocation.

```powershell
python research/native_workbench.py run pc-scene-manager-world --deadline-utc 2026-09-07T16:00:00Z
```
