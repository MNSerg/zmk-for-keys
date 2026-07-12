# ZMK Corne v3 (nice!nano) — PAT912x + OLED

Прошивка split-клавиатуры Corne v3: RGB, OLED на левой половине, трекбол PAT912x на правой.

## Документация

| Документ | Содержание |
|----------|------------|
| [docs/01-nastrojka.md](docs/01-nastrojka.md) | Где и что настраивается в файлах репозитория |
| [docs/02-ispolzovanie.md](docs/02-ispolzovanie.md) | Прошивка, ПК, OLED (`R:OK` / слой), слои, софт |

## Быстрый старт

1. Дождитесь сборки **Build ZMK firmware** в GitHub Actions.
2. Прошейте `corne_v3_left*.uf2` на левый nice!nano, `corne_v3_right*.uf2` на правый.
3. На OLED слева: **`R:OK`** = правая на связи, **`L0 DEF`** = активный слой.

Пин-аут: [`pinout nicenano.txt`](pinout%20nicenano.txt).
