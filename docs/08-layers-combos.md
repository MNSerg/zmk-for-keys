# Слои и комбо

Раскладка: `config/corne_v3.keymap` **и** дубликат `config/boards/shields/corne_v3/corne_v3.keymap` (держать синхронно).

## Слои

| # | Имя | Вход |
|---|-----|------|
| 0 | DEF | база; pos **40** = Delete (forward) |
| 1 | LOW | hold Space (`&lt 1`): F1–F12, grave над F1, RGB_TOG под F1 |
| 2 | RAI | hold Enter (`&lt 2`); **или sticky-комбо 38+40**; pos **11** = Insert |
| 3 | ADJ | hold `&mo 3`; справа — макросы ([12-macros.md](12-macros.md)) |
| 4 | CFG | комбо **36+41**; выход — то же или `&to 0` |

### LOW

- Верхний ряд: **Grave** на позиции F1-колонки, далее цифры…
- Средний ряд: **F1…F12**
- Под F1: **RGB_TOG**

### RAI

- Hold Enter — моментный слой.  
- Комбо **38+40** — постоянный (toggle), как CFG.  
- Pos 11 — **Insert**. Трекбол = скролл.

### ADJ

Левая: Studio / BT / USB / EXT_POWER.  
Правая: макросы — см. [12-macros.md](12-macros.md).

## Combos

| Комбо | Позиции | Действие |
|-------|---------|----------|
| cfg | 36, 41 | `&tog 4` |
| rai | 38, 40 | `&tog 2` |
| скобки и др. | см. keymap | |
