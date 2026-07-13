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
.\deploy.ps1 left          # левая
.\deploy.ps1 right         # правая
.\deploy.ps1 right_bare    # диагностика правой без трекбола
.\deploy.ps1 -Uf2 .\path\to\file.uf2
```

4. На OLED слева: **`R:OK`** = правая на связи; слой = `DEF` / `LOW` / …
5. ZMK Studio: USB → левая → ADJ → `&studio_unlock` → [zmk.studio](https://zmk.studio/).

Пин-аут: [`pinout nicenano.txt`](pinout%20nicenano.txt).
