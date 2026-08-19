# Трекбол PAT912x

## Пины (правая половина)

| Сигнал | nice!nano |
|--------|-----------|
| SDA | **D2** |
| SCL | **D3** |
| VCC | **VCC 3.3V** (не RAW/5V) |
| GND | GND |
| MOTION | не используется (часто 0 V) |

Драйвер: `pat912x_poll.c`. Синий LED MCU = probe OK.

| Файл | Роль |
|------|------|
| `corne_v3_right.overlay` | узел `trackball@75`, `res-x`/`res-y` |
| `pat912x_poll.c` | init + poll |
| **`trackball_tuning.h`** | **скорость, оси, инверсии (править здесь)** |
| `corne_v3_left.overlay` | `&trackball_listener` подключает tuning |
| `corne_v3.dtsi` | split listener stub |

## Гибкая настройка — `trackball_tuning.h`

Файл: `config/boards/shields/corne_v3/trackball_tuning.h`  
После правок прошейте **левую** половину.

### Скорость

`MUL/DIV` — только целые. Сейчас:

| Режим | Формула | Смысл |
|-------|---------|--------|
| Мышь | `8/5` | ×**1.6** |
| Скролл (RAI) | `1/6` | ÷**6** |

Примеры: `9/5` = ×1.8, `2/1` = ×2, `1/1` = ×1, `1/8` = ещё медленнее скролл.

### Оси и инверсии

Флаги (можно OR `|`):

| Флаг | Эффект |
|------|--------|
| `INPUT_TRANSFORM_XY_SWAP` | поменять X и Y |
| `INPUT_TRANSFORM_X_INVERT` | инвертировать X |
| `INPUT_TRANSFORM_Y_INVERT` | инвертировать Y |
| `0` | без трансформа |

Сейчас мышь: **XY_SWAP | Y_INVERT**. Скролл: `0` (отдельные флаги в `CORNE_SCROLL_TRANSFORM`).

Пример — только инверт Y без swap:

```c
#define CORNE_MOUSE_TRANSFORM (INPUT_TRANSFORM_Y_INVERT)
```

Пример — скролл с инвертом колеса по вертикали:

```c
#define CORNE_SCROLL_TRANSFORM (INPUT_TRANSFORM_Y_INVERT)
```

(после mapper Y → WHEEL, invert Y до mapper даёт инверт вертикального скролла.)

## Слой скролла

Скролл активен на слое **RAI (2)** — hold Enter или sticky-комбо 38+40. См. [08-layers-combos.md](08-layers-combos.md).

## CPI датчика (правая)

`res-x` / `res-y` в `corne_v3_right.overlay` (сейчас `0xbc`). Тонкая подстройка удобнее через `trackball_tuning.h`.

## Troubleshooting

| Симптом | Что сделать |
|---------|-------------|
| Нет курсора | VCC 3.3V, `R:OK`, dual-flash |
| Скорость | `CORNE_MOUSE_SCALE_*` / `CORNE_SCROLL_SCALE_*` |
| Оси | `CORNE_MOUSE_TRANSFORM` / `CORNE_SCROLL_TRANSFORM` |
