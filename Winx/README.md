# Реконструкция игрового слоя Winx

Этот корень отделяет классы `wx...` от библиотечного кода `Sparkplug/`.
Частичный PDB path подтверждает оригинальный game root
`Z:\Winx PS2\CODE\Build\PC\Release\WinxPC.pdb`, однако пути конкретных
translation units пока не найдены. Поэтому текущие `Code/PC` и `Code/PS2` —
явно inferred расположение, а не заявление о точном исходном дереве.

Первый срез содержит `wxPCApp` и `wxPS2App`: concrete игровые реализации
platform lifecycle shells. Реальные GUI, SDK и global-manager side effects
сохранены как документированные native boundaries и не запускаются в тестах.
