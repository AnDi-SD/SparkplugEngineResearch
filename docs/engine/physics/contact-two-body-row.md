# ContactConstraint: строка с двумя заданными body

Отдельная PC value-операция вычисляет ограниченную строку ContactConstraint
для двух заданных body. Она использует тот же полный contact block 228B,
два raw96B буфера `6C/70` и пять raw12B массивов `74/78/7C/80/84`.
Вся проекция имеет480B; неизвестные bytes и невыбранные элементы сохраняются.
Это собственное freevalue API, без native graph, pointer ABI и constructor.

Профиль фиксирует полный DWORD `40=0`, `58=1`, полный DWORD `88=0`, два
разных ненулевых body-токена `48/4C` и два разных row-токена `6C/70`.
Остальные output role-токены ненулевые. Токены инертны: указатели из них не
создаются и не разыменовываются. Две точки body передаются значениями вместо
неопознанных readonly44B carriers. Значения captured Manager `7C=3E4CCCCD`
и `80=3727C5AC` задаются явно; singleton и native Manager не используются.

Scalar положителен в `[0.25,4]`; все координаты, normal и rawCC конечны,
с модулем не больше16. Normal не обязан быть единичным. Signed zero и
subnormal сохраняют свои raw bit patterns. Default и неконечные значения
вне аналитического допуска отказывают до изменения output; это не заявление
о native fault/rollback контракте. Перед выдачей результата создаётся staged
copy всего значения, поэтому `before` и `output` могут совпадать.

## Асимметрия арифметики

Для первого body все три разности contact point−body point сохраняются в F32
и затем перечитываются. Для второго body X также проходит F32 store/reload,
а Y/Z остаются nearest64 extended на x87. Их промежуточные F32 copies не
используются в последующих произведениях. Поэтому второй angular row нельзя
получить как простое отрицание первого, даже при одинаковых body points.

Для второго body сначала вычисляются extended `y*nz−z*ny`, `z*nx−nz*x`,
`ny*x−y*nx`. X/Y проходят F32 store до смены знака; у Z знак меняется до
финального F32 store. Native store order — Z, X, Y; linear row имеет -normal.
Разность Y `3F800001−B3800000` при deltaZ=1 и NY=NZ=1 даёт J1 angularX
`34800000`, но J2 angularX `B4400000`; значение `B4800000` здесь неверно.
Зеркальный пример действует для Z/Y. Общие nearest64/F32 kernels и прежняя
[однострочная операция для одного body](contact-single-body-row.md) сохранены.

Выбрана одна ветвь native `48B610` с actual warm base `48B3C0` и двумя body.
Остальные native branches, friction/tangent/restitution, второй и последующие
rows, native C++ return/EAX, carrier class, physical base, startup, graph,
ownership, lifetime, PS2 и gameplay остаются открытыми. Это не новый
зарегистрированный класс и не завершение общего solver.
