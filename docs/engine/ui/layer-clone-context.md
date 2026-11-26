# Уточнение Clone слоёв и контекста CloneManager, 10 сентября 2026

## Почему MovieLayer получает null payload

MaterialTextureLayer Copy004235E0 сначала удаляет прежний payload назначения,
затем вызывает wrapper00467BC0 → CloneManager00412BE0 для payload источника.
`spMaterialMovieTexture` vtable006EA94C имеет original Clone004A1BF0, который
возвращает0. Layer Copy сохраняет этот0 в destination+10 и всё равно возвращает
true. Поэтому сам новый MovieLayer20 возвращается успешно с null payload.
Это реальное поведение игры; отсутствующий Clone не заменялся нашим кодом.

## Контекст после удаления слоёв

Movie/CubeEnv/Environment освобождают все собственные и контекстные allocations.
CameraView оставляет лениво созданный spPCRenderTargetManager68 (table006F27E4)
и его три16-байтных allocation. Свежий повтор отдельно вызывает original
manager delete: всё освобождено, remaining=0.

## Исправления прежних формулировок

Scouting labels `render-target-manager-factory` и `engine-core-factory` в
context-startup не являются правильными входами соответствующих factories и
исключены из атрибуции. Identity retained objects установлена независимо по
original vtable, exact getter и registration record; не по этим подписям окна.

## Собственное поле Copy у `spDXEnvironmentMapLayer`

У PC-слоя `spDXEnvironmentMapLayer` (`0x3F424FCD`) слот `+0x0C` таблицы
`0x006F0B78` ведёт к `Copy` по адресу `0x004C1120`. После успешного
копирования базовой части две инструкции читают четыре байта `+0x14`
источника и записывают их по тому же смещению назначения. Байты переносятся
без преобразования. Остальные байты обоих 24-байтных префиксов сохраняются.
Поле `+0x10` участвует в унаследованном копировании и остаётся за границей
этого участка. [Аналитический исходник](../../../Sparkplug/Code/SparkplugDX/spDXEnvironmentMapLayer.h)

Аналитический вызов принимает два раздельных префикса с основной таблицей
этого PC-слоя. Он сообщает о допустимости входов и копирует только слово
`+0x14`. Участок заканчивается перед чтением глобальной переменной
менеджера. В реестре PC базовым классом указан `spEnvironmentMapLayer`
(`0x427C7480`).
