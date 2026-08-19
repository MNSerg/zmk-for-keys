# Питание: EXT_POWER, soft-off, VCC

## EXT_POWER

Узел `/EXT_POWER` включает VCC для периферии (OLED, RGB, трекбол на VCC).

| Файл | |
|------|--|
| left overlay | `init-delay-ms = <200>` |
| right overlay | `init-delay-ms = <1000>` (датчику нужнее settle) |

Клавиши: LOW/ADJ — `EP_TOG` / `EP_ON` / `EP_OFF`.  
`EP_OFF` обесточит трекбол на VCC. Состояние пишется в settings.

RGB **не** должен рубить EXT_POWER: `CONFIG_ZMK_RGB_UNDERGLOW_EXT_POWER=n`.

## Soft-off

CFG → `&soft_off` (удержание). `CONFIG_ZMK_PM_SOFT_OFF=y`.  
Пробуждение: RESET на nice!nano **этой** половины (не путать с deep sleep от idle).

## Трекбол и питание

Только **3.3V VCC**. RAW/5V убивает PAT912x. См. [05-trackball.md](05-trackball.md).
