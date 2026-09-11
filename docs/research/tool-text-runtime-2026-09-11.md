# Text CPU runtime и правильный владелец Font atlas

Блок11 цикла до19:00, 11 сентября2026. Добавлены общие `spTextNode`,
`spTextRenderable` и `spTextNodeSerializer`; существующий Text serializer
обслуживает runtime и metadata одним field reader. Это **CPU layout/ownership**,
не отрисовка шрифта и не полная готовность Viewer.

## Оригинальные контракты

- PC Font measure462C00 работает с unsigned bytes20..FF. Newline увеличивает
  UInt32 height; остальные control bytes не добавляют ширину. Проверка переноса
  `currentWidth > wrap` выполняется перед очередным не-newline байтом, в том
  числе control byte. `ABAB` при A5/B7: wrap5 → width12/height24; wrap12 →17/24.
  Empty/NULL возвращает0, оставляя переданный height output неизменным.
- FontManager41E770 выбирает explicit Font или **borrowed member28**, записывает
  borrowed current2C и ARGB30 без изменения refcounts. Это отдельные поля от
  owning default references34/38. Manager41E730 делегирует measure; NULL current
  возвращает0 и явно обнуляет height output.
- Text factory41A640: размерA4, vtable6DEC60, строка/fontNULL, colorFFFFFFFF,
  wrap/align/width0, sphere0. AABB8C..A0 **не инициализирован**.
- Text setter4380F0 использует strlen/copy с первым NUL. NULL input действительно
  падает при чтении453ED4: runtime host guard отклоняет его и byte strings без
  ограниченного терминатора. Metadata сохраняет все исходные wire bytes.
- Layout437EE0 измеряет строку, обновляет sphere и X/Z extents; все ширины и
  высоты — UInt32. PC сначала округляет font height/baseline в float. При align2
  original43801C пишет **положительную** полную ширину в minimumX, maximumX0;
  это не исправлено под привычный AABB. Empty layout обнуляет sphere/width/min,
  но **не меняет max**: PC дважды пишет minimum. Неизвестные bounds представлены
  optional; старый void bounds API возвращает явно документированный NaN sentinel.
- Text reader441C10 выполняет layout после полей0/2/3/4, но не после color1.
  Повторное назначение того же Font всё равно вызывает layout. Порядок полей
  сохранён; отсутствие manager/default font/baseline сообщает явную host ошибку.
- TextNode factory41A5E0:1DC bytes, own1D4/1D8NULL. Append437970 всегда добавляет
  slot; последний Text дополнительно удерживается owning cache1D8. Дубликаты
  остаются отдельными slots. Detach437A10 сбрасывает cache только при совпадении;
  он не выбирает другой оставшийся Text. Clear4379D0 освобождает cache и slots.
  RenderNode attach/detach/clear стали virtual, чтобы общий reader выполнял эти
  настоящие derived операции. Serializer46253465 read4423D0/write4423B0/index
  4423C0 непосредственно делегирует RenderNode, отдельной wire секции нет.

## Исправленная ошибка реконструкции Font

Прежний `spFont::image_` ошибочно имел тип `spTextureData`. Original4426D5
передаёт expected class **spTexture2F281E13**, после resolver4678B0 удерживает
runtime pointer в+1C. PC header factory для TextureData создаёт DXTexture,
поэтому прежний dynamic cast отклонял правильный атлас.

Новая fresh проба создала настоящий DXTexture4AB520 (4C bytes/vtable6EF6E8),
Font462EC0 и serializer442500. Reader442660 получил этот объект через явно
обозначенную reference leaf, поднял refcount0→1 и записал точный pointer.
Font destructor освободил атлас; исходные device AddRef/Release сбалансированы.
Проверка0,499s, arena56528B. Исправлены shared owner и expected class в общем
Font reader; inspector необработанных atlas references сохранён.

## Проверки и границы

Основной original batch:9 measure inputs,4 последовательных reader states,
8 layout states,6 TextNode transitions. Последняя квалифицированная проба
`original-run4`:1,762s, arena10656B, scoped объекты освобождены. Runtime fixture
5212B, SHA256 `1F7101554A1C229BAC4DADF29CD957E24834954954DBA15009FB8B4C8A278518`.
Native regression сравнивает **246 original words**, включая точные float bits,
reference multiplicity и порядки slots;672 checks прошли, включая runtime atlas
ownership после исправления Font. FontSerialization,
TextInspection, RenderNode и FullLoader affected suites также прошли.

Fixtures используют прежние allocation/stream/name/COM leaves. Font reference
resolver — явная leaf к настоящему заранее прочитанному объекту; whole original
SMO loader/startup не заявлены. TextNode teardown использует borrowed zero
renderer page, проверяет сброс совпадающего cache pointer. Общие lazy allocations
за scoped объектами не объявлены полностью освобождённой игровой инфраструктурой.

PS2 identity/base/serializer registration уже известны; новые measurement/layout
пробы PC не переносятся на PS2 автоматически. Tool registration ограничена
PC/common masks7. Clone, Text writer, auxiliary TextNode producer, full text
render и default-font initialization остаются открытыми.

Whole-file Icy:119 objects/88 Nodes, graph+scene+lights на итоговой DLL прошли за0,045s. Реальный
`Media/Menus/menu.smo` теперь проходит TextNode registration, но доходит до
**legacy atlas ID30**, где strict common texture reader отказывает. Это отдельная
ранее известная [граница](tool-legacy-texture-source-boundary-2026-09-10.md).
Полная загрузка меню этим блоком **не объявляется завершённой**. Следующий блок —
явный host adapter сохранённых legacy pixels с общей реализацией декодера и
диагностикой; он не будет выдан за доказанное поведение старой игры.

Сохранены неуспешные пробы: NULL input; первоначально неверная ожидаемая texture
class в fixture; source mask6 вместо необходимого common1; неинициализированный
memory stream нового writer test и отсутствовавшая wire TextureData RTTI
registration нового atlas test. Исправлены ошибочные стартовые RTTI данные
раннего Text fixture: Text self-match не обращался к ошибочному parent record;
повтор с правильным75E030 сохранил fixture побитно. Неудачи не удалены.

Данные и выбранные source bindings: [manifest](../../research/tools-core-text-runtime-2026-09-11.json).
Это новая интеграция/проверка нужных CPU методов; EXE score не пересчитывался.
