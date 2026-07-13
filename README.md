# ZMK Corne v3 (nice!nano) — PAT912x + OLED

Прошивка split-клавиатуры Corne v3: RGB, OLED на левой половине, трекбол PAT912x на правой.

## Документация

| Документ | Содержание |
|----------|------------|
| [docs/01-nastrojka.md](docs/01-nastrojka.md) | Где и что настраивается в файлах репозитория |
| [docs/02-ispolzovanie.md](docs/02-ispolzovanie.md) | Прошивка, ПК, OLED (`R:OK` / слой), слои, софт |

## Быстрый старт

1. Дождитесь сборки **Build ZMK firmware** в GitHub Actions.
2. Скачайте `firmware.zip` → распакуйте в `firmware\` рядом с репо (или в Downloads).
3. Дважды RESET на nice!nano → диск **F:** (NICENANO) → прошивка:

```powershell
.\deploy.ps1 left
.\deploy.ps1 right
.\deploy.ps1 right_bare
.\deploy.ps1 left_bare
.\deploy.ps1 left_nostudio
.\deploy.ps1 settings_reset
.\deploy.ps1 -Uf2 .\path\to\file.uf2
```

Если справа не печатает даже на `right_bare` — сначала `settings_reset` на **обе**, затем `left_bare` + `right_bare` (см. docs). Studio-keymap на left может «убить» правые позиции при живом `R:OK`.

4. На OLED (полный left): **`R:OK`** = правая на связи.
5. ZMK Studio: USB → left → ADJ → `&studio_unlock` → [zmk.studio](https://zmk.studio/).

Пин-аут: [`pinout nicenano.txt`](pinout%20nicenano.txt).
