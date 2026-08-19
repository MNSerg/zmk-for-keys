# OLED статус

## Левая (всегда в полной сборке)

- SSD1306 128×32, I2C D2/D3, addr **0x3C**
- Экран: `custom_status_screen.c` (battery `NN%`, output USB/BTn, слой, `R:OK`/`R:--`)

Вкл.: `corne_v3_left.conf` → `CONFIG_ZMK_DISPLAY=y`, custom screen + LVGL fonts.

## Правая (опционально)

По умолчанию OLED **выкл.** (`corne_v3_right.conf` → `CONFIG_ZMK_DISPLAY=n`).

Включить: в `build.yaml` shield `corne_v3_right corne_v3_right_oled` (тот же I2C, что трекбол).

Слой/выход на правой OLED приходят с левой через `&st_sync` (`status_sync.c`).

## `st_sync`

| Что | Где |
|-----|-----|
| Behavior | `st_sync` в keymap |
| Код | `status_sync.c` |
| Также | флаг ACTIVE для idle — [03-idle-sleep.md](03-idle-sleep.md) |

## Надписи слоёв

`display-name` в keymap: `DEF`, `LOW`, `RAI`, `ADJ`, `CFG`. На peripheral имена из DT (не runtime keymap).
