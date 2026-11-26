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

### Добавление SkyBox в существующий список на PC

Внутренняя процедура `479EE0` добавляет запись в хвост списка менеджера.
Она выделяет отдельный 12-байтовый узел с заимствованным адресом SkyBox,
увеличивает счётчик и обновляет ссылки sentinel и прежнего хвоста.
Повторный адрес получает отдельный узел. Порядок сохраняется, а состояние
самого SkyBox и его счётчик ссылок остаются прежними.

[Аналитический источник](../../../Sparkplug/Code/Sparkplug/spSkyBoxManager.cpp)
воспроизводит успешное добавление на собственной структуре списка. При
допуске он проверяет ненулевой захваченный адрес и точный class ID
`0x7A7124AF`. Сопоставлены пустой и непустой списки, включая повторные
адреса. Защищённый вход, освобождение записей оригинала, ветвь отказа
аллокации, фабрика и подключение к рабочей Scene остаются открытыми.

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

## Участие LensFlare в списке менеджера на PC

При присоединении Scene проверяет точный class ID `0x435370B5` и вызывает
слот `+0x2C` менеджера из поля Scene `+0x28`. В PC таблице этот слот ведёт
через `4C5DC0` в `4CE9D0`. Метод добавляет flare в хвост, устанавливает
обратную ссылку и увеличивает счётчик. Общий unlink `4CEA10 → 4CE970`
обновляет начало и конец списка при необходимости, сшивает соседей,
обнуляет обе ссылки удалённого элемента и уменьшает счётчик.

Менеджер хранит head, tail и count в `+0x18`, `+0x1C`, `+0x20`.
В объекте flare предыдущий и следующий элементы находятся в `+0x58` и
`+0x5C`. Обе операции работают с заимствованными ссылками. Счётчик
владения flare остаётся прежним.

[Аналитический источник](../../../Sparkplug/Code/Sparkplug/spLensFlareManager.cpp)
воспроизводит эти операции на согласованном списке с отдельными записями
для захваченных адресов. Запись должна соответствовать точному классу
`spLensFlare` и оставаться доступной до unlink. Повторное включение,
нулевой адрес, другой класс и удаление из чужого списка отклоняются
до изменения состояния. Эти ограничения задаёт аналитический интерфейс.
В оригинальном append поиска дубликатов нет.

Сопоставлены добавление в пустой и непустой список, удаление головы,
середины, хвоста и единственного элемента, а также перенос между
двумя менеджерами через unlink и append. Очистка query-map в PC
обёртке удаления `4C6880`, жизненный цикл менеджера, подключение к
рабочей Scene и поведение PS2 остаются открытыми.

### Инициализация PC при пустой карте запросов

`4C5E30` вызывает сброс `4C67B0`. Для существующей пустой карты
sentinel остаётся на месте, три ссылки снова указывают на него, счётчик
записей равен нулю, а enabled сбрасывается. На этой ветви
счётчик созданных запросов и прежний байт capability сохраняются
до следующего вызова.

После сброса `4C5E30` берёт заимствованное устройство Renderer и
вызывает `CreateQuery` для типа `9` с нулевым выходным указателем.
Менеджер записывает capability `0` при точном ответе `0x8876086A`
и `1` при любом другом результате, включая `0x80004005`. Затем
он записывает enabled `1` и возвращает `AL = 1`.

[Аналитический источник](../../../Sparkplug/Code/SparkplugPC/spPCLensFlareManager.cpp)
принимает известную PC таблицу, пустую карту с существующим nil sentinel
и заимствованный адрес устройства. Синхронный поставщик передаёт
сырой HRESULT без передачи или приобретения query object. Для другой
формы карты операция отказывает до изменения состояния. Ветвь
освобождения элементов непустой карты, создание менеджера и жизненный
цикл устройства остаются отдельными.

## Границы описания

Открыты: original source/API names;58DA4026; complete native clone transaction;
LensFlare occlusion/glare/render; Projection helper4C51B0/virtual38;
Scene45EC70/SceneInit, concrete Partition/Occlusion types и portable scene wiring.
