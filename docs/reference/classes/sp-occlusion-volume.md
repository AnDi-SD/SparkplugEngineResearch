# spOcclusionVolume

Описание отдельных известных частей класса. Наличие карточки не означает полного восстановления всех методов.

Общие исходники: [spOcclusionVolume](../../../Sparkplug/Code/Sparkplug/spOcclusionVolume.h).

## Назначение и наследование

`spOcclusionVolume` — размещённая геометрия для visibility occlusion. Класс
наследует `spNode`, поэтому локальные Position и optional Rotation лежат в
первой serializer-секции. Собственная секция хранит два обязательных portable
буфера: triangle-list индексы и position-only вершины.

## Serializer layout

```text
# section 0: spNode
field 0  Position       Vector3, обязательно
field 1  Rotation       Quaternion XYZW, optional
field 8  IsAnimated     Boolean = false, обязательно
field 0  terminator

# section 1: spOcclusionVolume
field 0  IndexBuffer    portable triangle index buffer, обязательно
field 1  VertexBuffer   portable position vertex buffer, обязательно
field 0  terminator
```

IndexBuffer:

```text
UInt32 primitiveType   = 2       # triangle list
UInt32 primitiveCount  = T       # число треугольников
UInt32 indexFormat     = 0       # UInt16
UInt16 indices[T * 3]

payloadSize = 12 + 6*T
```

VertexBuffer:

```text
UInt32 declaration     = 0       # position-only, stride 12
UInt32 vertexCount     = V
UInt32 flags           = 0
Vector3 positions[V]

payloadSize = 12 + 12*V
```

Это та же portable геометрическая схема, которая внутри combined field
используется `spMeshBV`, но здесь index и vertex buffer разделены на два поля.
Оба executable writer требуют наличия обоих буферов.

## PC/PS2

По canonical resource path и ordinal сопоставлены все 20 объектов каждой пары:

Отдельная platform-ветка decoder не нужна.

## Viewer и исследовательская база

Analyzer записал:

Optional Rotation и разные V/T — параметры одного класса, а не подтипы.
