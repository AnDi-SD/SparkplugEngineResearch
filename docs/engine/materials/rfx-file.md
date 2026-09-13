# PC RFX: файл, regex, ID, имя и создание template

## Установленный engine caller

Точные встроенные выражения:

```text
6F3E64 <RmStringVariable NAME="ID"(?:.*?)VALUE="(.*?)"/>
6F3E98 <RmDirectXEffect NAME="(.*?)" TYPE(?:.*?)>
```

## Инициализация библиотеки и предел доказательства

`spPCRFXFileLoader::LoadFileForAnalysis` использует восстановленный file-text
helper и общий `LoadDocumentForAnalysis`; передаёт владение готовым metadata
template вызывающему коду. Два фиксированных regex воспроизведены через
стандартную библиотеку C++, `.*?` заменён на byte-spanning class с тем же
наблюдавшимся поведением. External scanf callback сохраняет видимый контракт.
API/member/header names аналитические.
