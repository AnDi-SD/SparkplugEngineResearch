# PC shared DX payload: writer, cursor и failure boundary

## Изменение C++

Оба reader-а проверяют 64-bit сумму размеров, физический размер потока с
учётом logical origin и предел payload32 МиБ перед выделением памяти.
Последовательный helper потребляет payload полностью. Новый
`ReadContiguousPayloadForAnalysis` оставляет cursor после размеров и принимает
только zero-origin contiguous stream. Ошибки возвращаются безопасно, прежний
target сохраняется; native null dereference не копируется на host.
