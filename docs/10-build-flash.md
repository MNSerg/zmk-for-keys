# Сборка и прошивка

## CI

Push в GitHub → workflow **Build ZMK firmware** → артефакт `firmware.zip`.

Список целей: `build.yaml`.

| Артефакт | Назначение |
|----------|------------|
| `corne_v3_left` | Левая (OLED + Studio) |
| `corne_v3_right` | Правая (трекбол) |
| `corne_v3_right_oled` | Правая + OLED |
| `*_bare` / `left_nostudio` | Диагностика |
| `settings_reset` | Сброс settings |

Всегда прошивайте **left и right из одного** успешного run.

## `deploy.ps1`

```powershell
.\deploy.ps1 left
.\deploy.ps1 right
.\deploy.ps1 settings_reset
.\deploy.ps1 -Uf2 .\path\to\file.uf2
```

Дважды RESET → диск NICENANO (часто `F:`) → скрипт копирует UF2.

## Локальная сборка

Нужен west + ZMK по `config/west.yml`. Обычно достаточно CI.

## После смены только docs

Пересборка не обязательна. После overlay/keymap/conf/C — нужна новая прошивка соответствующих половин.
