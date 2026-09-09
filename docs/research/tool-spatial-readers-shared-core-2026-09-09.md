# Пространственные ресурсы в общем загрузчике tools

Блок8 цикла9 сентября до19:00. Нужен для применения настоящих Model/Material/
Texture связей уровня: прежний ResourceGraph не мог прочитать Alfea02 из-за
отсутствующих зарегистрированных пространственных классов. Это перенос
ресурсного ядра, не реализация всего Scene/visibility и не выпуск Viewer.

## Общие классы и чтение

Добавлены ресурсные срезы `spZone`, `spPartitionSystem`,
`spPartitionRenderable` и фактического PC-наследника `spPCPartitionRenderable`.
Существующие PartitionNode/BSP/ZonePortal/ZonePortalNode дополнены нужными
отношениями. Семь настоящих serializers читают исходные секции, используя
общие `spDataBlockSerializer`, `ReadReference`, Node и RenderNode readers.
Дублирующей C# реализации нового загрузчика нет; прежние C# spatial inspector
decoders этим блоком ещё не заменены.

Сохраняются факты исходной игры:

- PartitionSystem физически использует RenderNode, но его RTTI parent — Node;
  `IsKindOf(RenderNode)` возвращает false. Default flags70E00 и direct root.
- Zone roots и PortalNode portals заимствованы; повторные указатели допустимы.
  Partition children/root/payload удаляются напрямую; Zone/portal/static edges
  используют владение, эквивалентное intrusive references.
- Child получает обратный parent. Collision сначала обновляет world, затем
  получает две взаимные borrowed регистрации; повторы и swap-last removal
  сохранены. Удаление снимает обратные регистрации с любой стороны.
- NULL Zone очищает поле; NULL Static/PartitionRenderable сохраняет предыдущее
  состояние. NULL DestinationZone разрешён. Child/System/Collision/Portal и
  renderable внутри payload требуют ненулевой ссылки.
- BSP plane — отдельные16 raw bytes; polygon не пересчитывает plane. Portal
  строит plane исходным общим методом. Open сохраняет raw byte, включая7F.
- После каждого добавления renderable контейнер присваивает ему текущий
  debug color. Позднее изменение цвета контейнера не перекрашивает предыдущие
  элементы. Пустые секции, повторы и неизвестные поля не заменены требованиями
  канонического writer order.

Конструкторы/ownership были исследованы ранее в
[partition runtime](native-pc-partition-runtime.md),
[BSP runtime](native-pc-bsp-runtime.md),
[portal runtime](native-pc-zone-portal-runtime.md). Новая проверка исполняет
ровно нужные readers:44B660,44CEB0,44AF40,44D7A0,44DDE0,44E670,44ECF0.
Оригинальные строки подтверждают пути serializers, кроме BSP/ZonePortal, где
пути помечены inferred. Пути новых runtime TU также inferred.

## Владение и пределы приложения

`spSerializerReadContextForAnalysis` явно различает shared и direct-owned
семейства. Он публикует идентичность до чтения, сохраняет unique owner до
появления настоящего owning edge и передаёт его один раз. Borrowed FAT/Zone
ссылки не становятся владельцами. Переданные children продолжают учитываться
в лимите; циклы и второй direct owner отклоняются. Нет no-op deleters,
подставных ресурсов или второй версии классов. Оставшиеся самостоятельные
direct owners переходят в ResourceGraph.

Это host lifetime bridge, не новый игровой алгоритм. File/FAT/cache порядок и
отдельный порядок публикации подготовленного mesh сохраняются. Замена одного
direct child/root/payload другим в повторном поле явно отклоняется: оригинал
делает raw overwrite, а безопасная политика такого переустройства пока не
входит в срез. Повторная запись того же указателя разрешена.

При подключении классов Alfea02 достиг нашего прежнего host limit4096. В файле
4266 ресурсов; измеренный peak рабочего процесса на отказе —48 852 992 bytes.
Предел сделан настраиваемым в общем context; ResourceGraph задаёт8192. Сохранены
64 уровня глубины, вход до64MiB, пределы полей/геометрии и отдельные mesh budgets.
Это изменение ограничения приложения, не формата или поведения оригинала.

После изменения pristine Alfea02 загружен:4266 объектов,489 Node,root ID1,
около0,065s, peak49 524 736 bytes (примерно47,2MiB). Icy119/88 и BloomX149/102
также загружены. Замер включает вызов C ABI загрузки; peak относится к отдельному
процессу Python с DLL и входными буферами. Это не FPS, UI startup или whole Scene.

## Проверка и оставшиеся границы

`probe_pc_spatial_serializers.py` исполнил14 коротких original-PC readers на
настоящих factory objects: пустая секция и значимые отношения/повторы для
каждого класса. ReadReference не подменялся: использован настоящий FAT lookup
с явно опубликованными объектами. Исходная инициализация FAT/manager и CPU
renderer cache — объявленные входы стенда, не доказательство Windows startup.
Каждый случай имеет свежую64KiB arena,100k instructions/2s на вызов и30s
process limit. Max arena65520 bytes; после штатного teardown все отслеживаемые
original allocations освобождены. Capped entries не запускались повторно.

`validate_tools_spatial_readers.py` сравнил14 original states с общими C++
readers на тех же bytes; все совпали. SpatialSerialization74 checks включают
NULL/extent/child-slot/unique-owner guards. Дополнительные ReadReference checks
проверяют передачу владения, запрет циклов/второго владельца и настраиваемый
лимит до фабрики. FullLoader, RenderNode и CollisionCore проверяют соседние пути.
Точные hashes итоговых sources/binaries, короткой выборки и сборок фиксируются
в `research/tools-core-spatial-readers-block-2026-09-09.json` после их завершения.

В двух первоначальных probes ошибки принадлежали стенду: cleanup не включал
созданный collision/world helper manager, а capture renderable vector читал
support+0 вместо vector+4. Ошибки и исправленные результаты сохранены отдельно;
лимиты и оригинальные bodies не изменялись.

Открыто: Octree serializer; derived BSP/Octree collision/static insertion при
непустых соответствующих полях; Scene attachment/visibility; пространственные
writer/index/serializer clone operations; замена старых C# spatial inspectors;
фактическое подключение material/texture runtime к Viewer. Неподдержанные
операции не выдают успешный результат. Node.scene в загруженном ресурсном
графе остаётся constructor-null; Scene initialization здесь не выполняется.
PS2 wire evidence сохраняется в карточках классов, нового PS2 runtime запуска
в этом блоке нет. GPU/backend и пользовательские редакторские исключения
не изменялись. Все ядра tools ещё не завершены.
