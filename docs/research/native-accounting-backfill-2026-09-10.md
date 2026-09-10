# Учёт подтверждённой работы 9–10 сентября

Первый блок нового цикла до 19:00 МСК. Это **перенос уже полученных знаний
в независимый реестр**, а не новые original probes сегодняшнего цикла.
Неисполненные методы, незавершённые классы и host adapters не получают
зачёт полного восстановления. Исходные знаменатели и веса сохранены.

## Результат

| Показатель | До | После |
|---|---:|---:|
| PC весь каталог | 17,481583% | 18,884038% |
| PS2 весь каталог | 11,365639% | 11,497797% |
| PC классов с отдельной оценкой | 193/733 | 207/733 |
| PS2 классов с отдельной оценкой | 137/681 | 139/681 |

Внесено **37 assessments: 31 PC и 6 PS2**. Из них 14 PC и 2 PS2 раньше
не имели отдельной оценки. Три оценки сохранены численно, но дополнены
актуальным evidence и границами. Ни один класс не объявлен closed.

PC прирост **+1,402456 п. п.**, PS2 **+0,132159 п. п.** относится к исправлению
учёта. Две цифры после запятой не означают точность экспертной оценки до сотых.
Число оценённых классов включает частичное понимание и не равно числу готовых
исходных классов. Unknown поведение остаётся в знаменателе.

## Основания

- Font/Text: actual PC factories, raw glyph reader, byte-string setter и
  inherited serialization delegation. Метаданные Text не зачтены как полный
  runtime layout. Отдельные Font/Text serializers раньше были без оценки.
- StaticRenderObject: reader, writer, index и независимые world/inverse матрицы;
  пять original/source случаев. Большой Scene runtime остаётся открытым.
- MaterialColor: отдельно успешные PC factory/delete и живой resource graph;
  отдельно статические PS2 constructor/layout/destructor и color initializer.
- Occlusion: original shape/Init/reader одного настоящего объекта, shared shape
  и lifetime hazards. Sort policy и полный source Init не объявлены готовыми.
- Particle: real PC2 Init с 539 частицами и directed 127/128/129. Не зачтены
  все velocity/world/angle ветки, simulation/draw и пока отсутствующий общий producer.
- Navigation: четыре класса и четыре serializer readers, two-bit matrix,
  borrowed links, MeshSet bounds; 11 original captures. Маршрутизация не закрыта.
- MeshBV/CollisionMesh/FaceDataContainer/wxFaceData: actual ownership,
  readers/writers, raw поля и clone face data. Полные collision queries не закрыты.
- Renderer/MaterialTexture: actual PC default graph и cache subset; отдельный
  PS2 static producer. PS2 material raw color sources отличаются от PC и
  записаны отдельно, неизвестный power не подставляется нулём.
- PS2 mesh header/bounds имеют **оба** адресных evidence: actual PC calls и
  static PS2 instructions. PS2 texture metadata имеет собственное MIPS evidence;
  простое наличие PC counterpart не дало нового PC балла.

Результаты UI, удаление C# дублей, скорость harness и число managed assertions
сами по себе не повышали оценки классов. У уже хорошо изученных материалов
новое подтверждение оставило score прежним. Незарегистрированный geometry
helper не стал выдуманным RTTI-классом ради увеличения числителя.

## Проверка и воспроизведение

[Manifest](../../research/native-platform-backfill-2026-09-10.json) содержит
каждую оценку, её границы, original locators, неизвестные и 29 уникальных
evidence files с SHA256. Прежнее знание сохраняется ссылкой на неизменяемый
предыдущий platform manifest, без переписывания исторических документов.

Перед импортом скопированы только небольшие native tables в память; importer
проверил все 37 записей, платформы, хэши и foreign keys. Прогноз затем совпал
с реальным transactional import; все 37 записей повторно проверены read-only.
Полная corpus БД не копировалась и не сканировалась. Новых guest/GPU runs нет.

```powershell
python research/native_platform_knowledge.py import --manifest research/native-platform-backfill-2026-09-10.json
python research/native_goal_coverage.py report
```

Повторный импорт того же manifest идемпотентен. Локальные before/after,
preview и applied audit находятся в
`local-data/results/native-cycle-20260910-1900/backfill/`.

Этот пакет не является повторным аудитом всей истории проекта. Остальные
неучтённые результаты прежних циклов, включая LensFlare/Skybox/Octree reader
и отдельные collision/spatial методы, остаются адресной очередью учёта.
Это не отсутствие их сохранённых исследований и не разрешение снова выполнять
старые пробы без нового вопроса. Новые открытия цикла учитываются отдельно.
