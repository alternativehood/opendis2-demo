# archive.sh — source snapshot

`make archive` (или `./tools/archive.sh`) пакует snapshot текущего working tree в `.tar.gz`.

Имя архива: `opendis2-{sanitized_branch}-{timestamp}.tar.gz`.
Слешы в имени ветки заменяются на дефисы. Например, `agent2/work` → `agent2-work`.
Detached HEAD → `detached-head`.

- **не собирает** проект;
- **не тестирует**;
- **не требует** deps (vcpkg, система);
- может паковать сломанный код — это просто срез для ревью, CI или передачи.

Архив кладётся в `archive_artifacts/` (директория в `.gitignore`).
