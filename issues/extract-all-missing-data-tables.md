# extract-all не обрабатывает DBF / DAT / DLG

## Симптом

`extract-all` заявлен как «извлечь все игровые ассеты за один проход», но пропускает
все файлы данных: `.dbf` (игровые таблицы), `.dat` (конфиг), `.dlg` (диалоги UI).
Их можно извлечь только отдельными командами (`extract-dbf`, `extract-dat`,
`extract-dlg`) вручную.

## Причина

`commands_extract_all.cpp` фильтрует контейнеры по `ContentKind`:
только `Images`, `Animations` и `Sounds` попадают в обработку.
`GameScanner` помечает `.dbf`/`.dat`/`.dlg` как «unknown content type» →
они попадают в массив `skipped` в итоговом `extraction_manifest.json`
и не обрабатываются.

## Последствие

После `extract-all` нет единой выходной директории с полным набором ассетов игры.
Для Stage 1 (`runtime-asset-manifest-v1`) это означает, что верхний манифест
не может автоматически ссылаться на извлечённые таблицы данных — их нужно
прогонять отдельно и интегрировать вручную.

## Что нужно сделать

Два варианта:

1. **Расширить `extract-all`** — добавить в цикл обработку файлов с известными
   расширениями (`.dbf`, `.dat`, `.dlg`) через соответствующие `cmd_extract_*`.
   Потребует обновления `ContentKind` или отдельного сканирования `other_files`
   из `ScanResult`.

2. **Оставить как есть** и задокументировать ограничение — если runtime asset layer
   не включает game data tables (scope покрывает только graphical assets + sounds),
   отдельные команды достаточны.

Решение зависит от того, войдут ли DBF/DAT/DLG в scope `libd2asset` или это
будет отдельный `libd2gamedata`.

## Затронутые файлы

- `src/cli/commands_extract_all.cpp` — основной цикл обработки контейнеров
- `src/d2res/game_scanner.hpp` / `game_scanner.cpp` — классификация `ContentKind`

## Статус

**Выполнено** — реализовано в рамках OpenSpec change `extract-all-missing-data-tables`.
- Добавлен `ContentKind::DataTables` и диспатч через `extract-all`
- Обновлены манифесты (`extraction_manifest.json`, `game_manifest.json`)
- Документировано в `docs/formats/extract-all.md` и `docs/formats/content-kind.md`
- Детали содержимого таблиц задокументированы в `docs/formats/runtime_data_tables.md`
