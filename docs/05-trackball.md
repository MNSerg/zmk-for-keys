# Трекбол PAT912x

## Пины (правая половина)

| Сигнал | nice!nano |
|--------|-----------|
| SDA | **D2** |
| SCL | **D3** |
| VCC | **VCC 3.3V** (не RAW/5V) |
| GND | GND |
| MOTION | не используется (часто 0 V) |

Драйвер: `pat912x_poll.c` (poll I2C, без MOTION IRQ). Синий LED MCU = probe OK.

| Файл | Роль |
|------|------|
| `corne_v3_right.overlay` | узел `trackball@75`, `res-x`/`res-y` |
| `pat912x_poll.c` | init + poll |
| `corne_v3_left.overlay` | `&trackball_listener` — скорость, оси, скролл |
| `corne_v3.dtsi` | `trackball_split` / listener stub |

## Скорость и оси (настраивать на **левой**)

Файл: `config/boards/shields/corne_v3/corne_v3_left.overlay`

### Режим мыши (слой не RAI)

```
input-processors
  = <&zip_xy_transform INPUT_TRANSFORM_XY_SWAP>  /* X↔Y */
  , <&zip_xy_scaler 3 1>                         /* ×3 быстрее */
  ;
```

- `zip_xy_scaler MUL DIV` → скорость ×(MUL/DIV). Быстрее: `4 1`, `5 1`. Медленнее: `2 1`, `1 1`.
- `INPUT_TRANSFORM_XY_SWAP` — поменять оси. Инверсия: `INPUT_TRANSFORM_X_INVERT` / `Y_INVERT` (можно OR битов).

### Режим скролла (слой **RAI** = 2)

```
scroll {
  layers = <2>;
  input-processors
    = <&zip_xy_scaler 1 4>           /* ÷4 медленнее */
    , <&zip_xy_to_scroll_mapper>
    ;
};
```

Медленнее скролл: `1 6`, `1 8`. Быстрее: `1 2`, `1 1`.

Слой 2 должен быть активен (hold Enter / sticky combo RAI) — см. [08-layers-combos.md](08-layers-combos.md).

## CPI датчика (правая)

В `corne_v3_right.overlay`:

```
res-x = <0xbc>;
res-y = <0xbc>;
```

Выше значение ≈ выше «сырой» CPI (до scaler). Меняйте осторожно; тонкая подстройка удобнее через `zip_xy_scaler` на left.

## Прошивка

Всегда **обе** половины из одного CI zip. После смены только overlay left — достаточно перепрошить left; смена right overlay / драйвера — right.

## Troubleshooting

| Симптом | Что проверить |
|---------|----------------|
| Нет курсора | VCC 3.3V, `R:OK`, синий LED справа, dual-flash |
| Слишком медленно/быстро | `zip_xy_scaler` в left overlay |
| Оси не те | `INPUT_TRANSFORM_*` или `invert-x`/`invert-y` на trackball |
| Скролл не тот слой | `layers = <2>` и активность RAI |
