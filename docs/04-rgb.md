# RGB подсветка

## Железо

- Шина: SPI MOSI **D1 / P0.06**
- Цепь: **6 underglow + 21 per-key** = `chain-length = <27>` в `corne_v3.dtsi`
- Первые 6 (низ) всегда гасятся прокси `led_strip_blank` (`blank-count = <6>`)

| Файл | Что |
|------|-----|
| `corne_v3.dtsi` | `led_strip`, `led_strip_perkey`, `zmk,underglow` |
| `led_strip_blank.c` | blank + override для `&batt_bar` |
| `corne_v3_left.conf` / `_right.conf` | Kconfig RGB |

## Kconfig (типичные)

```
CONFIG_ZMK_RGB_UNDERGLOW=y
CONFIG_ZMK_RGB_UNDERGLOW_ON_START=y
CONFIG_ZMK_RGB_UNDERGLOW_AUTO_OFF_IDLE=y
CONFIG_ZMK_RGB_UNDERGLOW_EFF_START=0          # solid
CONFIG_ZMK_RGB_UNDERGLOW_BRT_START=40
CONFIG_ZMK_RGB_UNDERGLOW_BRT_MAX=60
CONFIG_ZMK_RGB_UNDERGLOW_EXT_POWER=n          # не резать VCC трекбола
```

## Управление с клавиш

| Где | Действие |
|-----|----------|
| LOW | `RGB_TOG` |
| CFG (верхний ряд) | TOG / BRI / BRD / HUI / HUD |

Яркость и цвет **сохраняются** в flash (~60 с debounce). Перекрывают `*_START`. Сброс: `settings_reset`.

## Idle

См. [03-idle-sleep.md](03-idle-sleep.md). Гаснет через 10 с; sync с левой гасит правую сразу.

## Если RGB «залип»

1. `RGB_TOG` на LOW/CFG  
2. Или `settings_reset` → обычный left/right UF2  
