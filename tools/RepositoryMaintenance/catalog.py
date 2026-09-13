#!/usr/bin/env python3
"""Build public navigation from Markdown and the public class registry.

No game files, private archive, SQLite database or network are used.
"""
from __future__ import annotations
import argparse
import json
import posixpath
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
START = '<!-- catalog:start -->'
END = '<!-- catalog:end -->'
CATEGORIES = {
    'architecture': ('Архитектура и жизненный цикл', 'Ядро связывает менеджеры и задаёт порядок обновления. Начните с [архитектуры](overview.md) и [PC-кадра](engine-frame.md).'),
    'objects': ('Объекты, владение и уведомления', 'Базовая объектная модель связывает идентичность, имена и время жизни. Регистрации и поля отдельных типов находятся в [справочнике классов](../../reference/classes.md).'),
    'resources': ('Ресурсы и сериализация', 'Загрузка восстанавливает объектный граф и его ссылки. Начните с [общего конвейера](pipeline.md); расположение байтов описано в [SMO/FFPS](../../formats/smo.md).'),
    'scene': ('Сцена и преобразования', 'Узлы хранят локальное состояние и кэш мирового преобразования. [Обзор узлов](overview.md) объясняет порядок матриц, наследование и обход детей.'),
    'geometry': ('Геометрия и буферы', 'Геометрия хранится в ресурсах mesh и платформенных буферах. Сериализованный stride, runtime stride, topology и атрибуты вершин описываются отдельно.'),
    'animation': ('Анимация и скелеты', 'Именованные кривые связываются с узлами; actor и контроллеры управляют воспроизведением, skin применяет палитру костей. Дисковые кривые описаны в [SAN](../../formats/san.md).'),
    'rendering': ('Отрисовка', 'Renderer получает преобразования, геометрию, материалы и состояние сцены. Описания PC-путей не означают тождественной реализации PS2 или нашего OpenGL backend.'),
    'materials': ('Материалы и шейдеры', 'Материал связывает проходы, текстуры, raw states и параметры shader. Один флаг или alpha текстуры не заменяет полного состояния связанного renderable.'),
    'textures': ('Текстуры', 'Источники и платформенные представления текстуры определяют чтение, mip-уровни и загрузку. [Формат встроенной PC-текстуры](../../formats/smo-textures.md) описан отдельно от ограничения writer конкретного приложения.'),
    'visibility': ('Видимость и пространственные структуры', 'Partition, BSP, octree, зоны и порталы определяют пространственные связи и отсечение. Наличие структуры в файле не доказывает полного описания всех runtime-ветвей.'),
    'physics': ('Физика и коллизии', 'Объёмы и collision-геометрия описывают данные столкновений. Физическая симуляция, response и игровые потребители — отдельные части поведения. [Обзор](overview.md).'),
    'effects': ('Свет, частицы и эффекты', 'Свет, fog и частицы связаны со сценой, мировыми преобразованиями и проходами renderer. Параметры ресурса и вычисления во время кадра рассматриваются раздельно.'),
    'ui': ('Текст и интерфейс', 'GUI связывает объекты, текст, фокус и маршруты сообщений. Игровые меню описаны также в [разделе игры](../../game/flow/README.md).'),
    'platforms': ('Платформенные реализации', 'Общие имена классов не устраняют различий layout, выравнивания и данных. Начните со [сравнения PC и PS2](pc-and-ps2.md).'),
    'characters': ('Персонажи и действия', 'Машина состояний выбирает поведение персонажа и связывает анимационные запросы с воспроизведением. Одинаковый constructor selector не доказывает одинакового поведения разных состояний.'),
    'flow': ('Состояния игры и меню', 'Game flow управляет состояниями приложения и уровня, а игровой GUI — меню и пользовательскими действиями. Здесь также описаны игровые камеры и разрешение.'),
    'triggers': ('Триггеры и взаимодействия', 'Триггер проверяет условия входа, выхода и действия. Установка его active-флага и фактическое появление окна HUD — отдельные события; подавление HUD не обязательно отменяет active-состояние.'),
    'assets': ('Игровые ресурсы', 'Модели, анимации, шаблоны и placements участвуют в разных частях игры. [Поиск файлов на PC](resource-loading.md) и [соседние форматы](../../formats/game-resources.md) задают общую картину.'),
    'ai': ('Искусственный интеллект', 'Семейства действий и поведения связывают команды, восприятие, навигацию и текущие дочерние действия. Описанные поля и начальные состояния не означают полного восстановления всех AI-решений.'),
    'entities': ('Игровые объекты', 'Игровые entity связывают состояние, регистрацию в менеджерах и узлы сцены. Копирование объекта требует учитывать ссылки и различия между обычным clone и игровым modified-copy.'),
}


def title(path: Path) -> str:
    for line in path.read_text(encoding='utf-8-sig').splitlines():
        if line.startswith('# '):
            return line[2:].strip()
    raise ValueError(f'Missing title: {path.relative_to(ROOT)}')


def link(label: str, target: str, source: str) -> str:
    return f'[{label}]({posixpath.relpath(target, posixpath.dirname(source))})'


def generated_files() -> dict[str, str]:
    generated = {}
    registry = json.loads((ROOT / 'docs/reference/classes.json').read_text(encoding='utf-8'))
    classes = registry['classes']
    for owner, heading in (('engine', 'Классы движка'), ('game', 'Классы игры')):
        source = f'docs/reference/{owner}-classes.md'
        rows = [f'# {heading}', '', '[Общий каталог](classes.md). «Да» означает присутствие регистрации в известной версии платформы. Прочерк в описании или исходниках означает отсутствие соответствующей ссылки, а не отсутствие класса в игре.', '', '| Имя | Class ID | PC | PS2 | Описание | Исходники |', '| --- | --- | --- | --- | --- | --- |']
        for item in classes:
            if item['owner'] != owner:
                continue
            description = ', '.join(link(str(n + 1), p, source) for n, p in enumerate(item['documentation'])) or '—'
            sources = ', '.join(link(Path(p).name, p, source) for p in item['sources']) or '—'
            rows.append(f"| `{item['name']}` | `{item['classId']}` | {'Да' if 'pc' in item['platforms'] else '—'} | {'Да' if 'ps2' in item['platforms'] else '—'} | {description} | {sources} |")
        generated[source] = '\n'.join(rows) + '\n'
    counts = {owner: sum(c['owner'] == owner for c in classes) for owner in ('engine', 'game')}
    pc = sum('pc' in c['platforms'] for c in classes)
    ps2 = sum('ps2' in c['platforms'] for c in classes)
    generated['docs/reference/classes.md'] = f'''# Каталог классов

Все **{len(classes)}** известных зарегистрированных имени: **{counts['engine']}** класса движка и **{counts['game']}** игровых. PC содержит {pc} регистрации, PS2 — {ps2}; платформенные списки частично пересекаются.

| Раздел | Содержание |
| --- | --- |
| [Классы движка](engine-classes.md) | Типы `sp…`, ID, платформы, описания и код |
| [Классы игры](game-classes.md) | Типы `wx…`, ID, платформы, описания и код |
| [Карточки классов](classes/README.md) | Подробные поля, layout и известное поведение |
| [Машиночитаемый реестр](classes.json) | Те же записи и регистрационные base ID для каждой платформы |

Class ID — идентификатор зарегистрированного типа. `registrationBaseIds` в JSON описывает регистрационную связь; физическое C++-наследование и ABI устанавливаются отдельно. Отсутствие в одной известной версии не доказывает отсутствия класса во всех сборках этой платформы.

Карточка описывает известные части класса, а ссылка на исходники — имеющуюся реализацию. Ни то ни другое не означает полного восстановления всех методов. Для поиска имени или ID используйте поиск по странице соответствующего списка либо JSON.
'''
    folders = sorted(p for area in ('engine', 'game') for p in (ROOT / 'docs' / area).iterdir() if p.is_dir())
    folders.append(ROOT / 'docs/reference/classes')
    for folder in folders:
        rel = folder.relative_to(ROOT).as_posix()
        source = rel + '/README.md'
        if folder.name == 'classes':
            heading, intro = 'Карточки классов', 'Подробные описания известных частей классов. Все зарегистрированные имена, включая типы без отдельной карточки, находятся в [общем каталоге](../classes.md).'
        else:
            heading, intro = CATEGORIES[folder.name]
        path = ROOT / source
        current = path.read_text(encoding='utf-8-sig') if path.exists() else f'# {heading}\n\n{intro}\n'
        base = current.split(START, 1)[0].rstrip()
        rows = [base, '', START, '', '## Статьи', '']
        files = sorted((p for p in folder.glob('*.md') if p.name != 'README.md'), key=lambda p: (title(p).casefold(), p.name))
        rows += [f'- [{title(p)}]({p.name}).' for p in files]
        rows += ['', END, '']
        generated[source] = '\n'.join(rows)
    # Include newly generated pages before they are written.
    paths = {p.relative_to(ROOT).as_posix() for p in (ROOT / 'docs').rglob('*.md') if p.name != 'catalog.md'} | set(generated)
    entries = []
    for path in sorted(paths):
        heading = generated[path].splitlines()[0][2:] if path in generated else title(ROOT / path)
        parts = Path(path).parts
        category = '/'.join(parts[1:-1]) or 'start'
        entries.append({'path': path, 'title': heading, 'category': category})
    generated['docs/catalog.json'] = json.dumps({'schemaVersion': 1, 'documents': entries}, ensure_ascii=False, indent=2) + '\n'
    rows = ['# Полный каталог базы знаний', '', 'Все публичные статьи сгруппированы по темам. [Начальная страница](README.md) предлагает маршрут чтения, [каталог классов](reference/classes.md) — поиск типа или ID.', '']
    groups = {}
    for entry in entries:
        groups.setdefault(entry['category'], []).append(entry)
    for group, items in groups.items():
        parts = group.split('/')
        heading = {'start': 'Начало', 'engine': 'Движок', 'game': 'Игра', 'formats': 'Форматы', 'reference': 'Справочник'}.get(parts[0], parts[0])
        if len(parts) > 1:
            heading += ' / ' + ('Карточки классов' if parts[1] == 'classes' else CATEGORIES[parts[1]][0])
        rows += [f'## {heading}', '']
        rows += ['- ' + link(item['title'], item['path'], 'docs/catalog.md') + '.' for item in sorted(items, key=lambda i: (i['title'].casefold(), i['path']))]
        rows.append('')
    generated['docs/catalog.md'] = '\n'.join(rows)
    return generated


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Report stale generated navigation without writing files.')
    args = parser.parse_args()
    stale = []
    generated = generated_files()
    for relative, content in generated.items():
        path = ROOT / relative
        if path.exists() and path.read_text(encoding='utf-8-sig') == content:
            continue
        stale.append(relative)
        if not args.check:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding='utf-8', newline='\n')
    if args.check and stale:
        print('Stale navigation:\n' + '\n'.join(stale))
        return 1
    print(f'Catalog: {len(generated)} generated files; {len(stale)} updated.' if not args.check else 'Catalog is current.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
