# extract-all: manifest.json от extract-anim перезаписывает manifest.json от extract-images

## Симптом

После `extract-all` в директории каждого `.ff`-контейнера лежит `manifest.json`
с полями `failed_animations`, `total_animations`, `written_animations`.
Статистика по изображениям (`total_images`, `written_images`) отсутствует —
она была записана раньше и перезаписана.

## Причина

`commands_extract_all.cpp` вызывает последовательно:

```cpp
cmd_extract_images(abs_path, out_sub.string(), "");   // пишет manifest.json
cmd_extract_anim(abs_path, out_sub.string(), ...);    // перезаписывает manifest.json
```

Обе команды пишут файл с одним именем `manifest.json` в один и тот же `out_sub`.
Нет никакой проверки на существующий файл — второй вызов молча перезаписывает первый.

## Последствие

- Потеряна статистика извлечения изображений (сколько декодировано, сколько упало).
- Любой инструмент, читающий `manifest.json` после `extract-all`, видит только
  анимационный манифест. Если он ожидает поля `total_images` — сломается.
- Для Stage 1 (`runtime-asset-manifest-v1`): если верхний манифест будет
  агрегировать per-container манифесты, данные по изображениям будут недоступны.

## Что нужно сделать

Три варианта:

1. **Разделить имена файлов** — `extract-images` пишет `images_manifest.json`,
   `extract-anim` пишет `anim_manifest.json`. Ломает текущий формат (minor).

2. **Объединять манифесты** — `extract-all` после обоих вызовов читает оба
   временных файла и мёрджит в единый `manifest.json` с полями для обоих типов.

3. **Убрать per-container манифест из extract-all** — пусть манифест пишет только
   `extraction_manifest.json` на верхнем уровне, а per-container файлы
   остаются в зоне ответственности отдельных команд. Проще всего, но меняет
   контракт `extract-all`.

## Затронутые файлы

- `src/cli/commands_extract_all.cpp` — строки 57–68
- `src/cli/commands_extract_images.cpp` — запись `manifest.json`
- `src/cli/commands_extract_anim.cpp` — запись `manifest.json`
