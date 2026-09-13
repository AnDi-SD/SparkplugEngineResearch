# MaterialColorController: PS2 constructor и граница PC

## Подтверждённый PS2 путь

Регистрация: `spMaterialColorController/4C633E85` наследует
`spRenderController/14477AC7`. Factory134480 выделяет1E0 и вызывает ctor11B060;
vtable48D720. Ctor вызывает RenderController11BD60, задаёт material24=NULL,
saved ambient/diffuse/specular/emissive RGBA28/38/48/58=(0,0,0,1).
Четыре ColorFunc в68/B8/108/158 используют vtable48CD90, два color endpoints
из global476CB0, затем FunctionEval11CDF0 в каждом leaf+18. Alpha1A8 тоже
инициализируется11CDF0. Scalar defaults: type0; amplitude/period/xScale1;
остальные offset/phase/seed words0, clamp byte0. Base render clock1C/20=0.

Global476CB0 в file image нулевой. Отдельный static initializer47F2A0
содержит `lui s4,FF00` и `sw s4,6CB0(v1)` при v1=00470000: значение
**FF000000**. Указатель47F2A0 находится по48BFCC рядом с другими initializer
addresses. Ноль из raw ELF больше не рассматривается как runtime color default;
полный startup и порядок исполнения всей таблицы здесь не запускались.

Deleting destructor11AE70 последовательно разбирает alpha1A8,
ColorFunc158/108/B8/68, их scalar/base parts, затем RenderController11BD00;
условное освобождение идёт через10D810. В этом PS2 теле не наблюдается
BindMaterial(NULL), восстановление цветов материала или владение material24.

## Что это разрешает и чего пока не доказывает

PS2 снимает неопределённость собственной layout/default/cleanup ветки и даёт точные ориентиры для PC. PC consumers ранее подтвердили те же offsets и leaf defaults, но его factory41A580 остаётся capped88CD04, destructor4372E0 также не подтверждён. Ни factory, ни вызывающий его clone заново не запускались. Полное совпадение конструкторов платформ не заявляется.

Общая PC resource factory сохранена NULL. Аналитический declared-state класс
не выдан за настоящий загруженный ресурс. Новых файлов с этим контроллером
в working graph нет. Следующий необходимый шаг — независимое подтверждение
PC constructor/default/ownership с использованием найденных anchors; если
потребуется альтернативная реализация, это отдельное предложение пользователю.
