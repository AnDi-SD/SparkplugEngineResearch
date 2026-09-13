# PC spSceneManager: borrowed list и world caller

45A7D0 идёт по списку, публикует scene20, вызывает scene14 root virtual30(0),
затем читает next. Scene24/25 и Node Enabled200 не фильтруют обход; return
root игнорируется. Current обнуляется также при пустом списке. Новые callback
сценарии подтвердили post-call next: append посещается в том же проходе,
remove-next пропускается; false-root не обрывает остальные вызовы.

Host guards: NULL root, duplicate, 4096 entries/visits, повторный Update и
удаление текущего элемента. False от source Node safety check агрегируется
без short-circuit; исходный native helper void не определяет такой результат.
Caller обязан сохранять borrowed views/root до unregister. Callback удаления
manager/current scene и произвольные typed reparent остаются открыты.
