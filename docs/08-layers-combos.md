# Слои и комбо

Раскладка: `config/corne_v3.keymap` **и** дубликат `config/boards/shields/corne_v3/corne_v3.keymap` (держать синхронно).

## Слои

| # | Имя | Вход |
|---|-----|------|
| 0 | DEF | база |
| 1 | LOW | hold Space (`&lt 1`) |
| 2 | RAI | hold Enter (`&lt 2`) / hold ltq-Bksp; **или sticky-комбо 38+40** |
| 3 | ADJ | hold `&mo 3` |
| 4 | CFG | комбо внешних thumbs **36+41** (`&tog 4`); выход — то же или `&to 0` |

### RAI: моментный и постоянный

- **Пока удерживаете** Enter (`&lt 2`) или ltq-Backspace — слой активен, отпустили — нет.
- **Комбо Enter+ltq-Bksp (позиции 38 и 40)** — `&tog 2`: слой остаётся, пока снова не нажмёте ту же комбо (как CFG).

На RAI трекбол = скролл ([05-trackball.md](05-trackball.md)).

### CFG

Комбо **36 + 41** (внешние большие пальцы обеих половин). RGB, `&batt_bar`, `&soft_off`.

## Combos (скобки и т.п.)

В keymap → `combos { ... }`. Позиции 0-based по матрице 42 клавиш (левая 0–20, правая 21–41 с offset).

| Комбо | Позиции | Действие |
|-------|---------|----------|
| cfg | 36, 41 | `&tog 4` |
| rai | 38, 40 | `&tog 2` |
| скобки и др. | см. keymap | |

Сменить sticky-RAI: другие `key-positions` у `rai_combo`.

## Hold-tap

- `hm` — homerow mods  
- `ltq` — layer-tap с длинным tapping-term  

Параметры в узлах `behaviors` keymap.
