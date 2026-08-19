# Макросы на слое ADJ (правая половина)

На слое **ADJ** (`&mo 3` на DEF) вся **правая** половина — макросы и сочетания клавиш. Левая — Studio / BLE / USB / EXT_POWER.

## Где править

Файл: `config/corne_v3.keymap` **и** дубликат  
`config/boards/shields/corne_v3/corne_v3.keymap` (держите одинаковыми).

1. Секция `/ { macros { ... } };` — определение макросов.  
2. Слой `layer_3` (ADJ) — какие клавиши вызывают `&m_…`.

После правок: CI → прошить **левую** половину (раскладка на central).

## Карта правой половины на ADJ

```
Верх:   copy  paste  cut   undo  redo  save
Сред:   selall find  new   win   close desk
Низ:    custom1 custom2 custom3  (свободны)
Thumbs: (trans)
```

| Узел | По умолчанию |
|------|----------------|
| `&m_copy` | Ctrl+C |
| `&m_paste` | Ctrl+V |
| `&m_cut` | Ctrl+X |
| `&m_undo` | Ctrl+Z |
| `&m_redo` | Ctrl+Shift+Z |
| `&m_save` | Ctrl+S |
| `&m_selall` | Ctrl+A |
| `&m_find` | Ctrl+F |
| `&m_new` | Ctrl+N |
| `&m_win` | Win+Tab |
| `&m_close` | Alt+F4 |
| `&m_desk` | Win+D |
| `&m_custom1/2/3` | F13 / F14 / F15 (заготовки) |

## Как записать свой макрос

### 1. Объявить узел

Внутри `macros { }`:

```dts
m_hello: m_hello {
    compatible = "zmk,behavior-macro";
    #binding-cells = <0>;
    bindings
        = <&macro_tap &kp H>
        , <&macro_tap &kp E>
        , <&macro_tap &kp L>
        , <&macro_tap &kp L>
        , <&macro_tap &kp O>
        ;
};
```

### 2. Повесить на клавишу ADJ

В `bindings` слоя ADJ на нужной позиции правой половины:

```dts
&m_hello
```

вместо `&trans` или другого макроса.

### 3. Типичные кирпичики

| Binding | Смысл |
|---------|--------|
| `&macro_tap &kp X` | нажать и отпустить X |
| `&macro_press &kp LCTRL` | зажать модификатор |
| `&macro_release &kp LCTRL` | отпустить модификатор |
| `&macro_wait_time 100` | пауза 100 ms (если доступно в вашей версии ZMK) |

**Ctrl+Shift+P (пример):**

```dts
bindings
    = <&macro_press &kp LCTRL>
    , <&macro_press &kp LSHFT>
    , <&macro_tap &kp P>
    , <&macro_release &kp LSHFT>
    , <&macro_release &kp LCTRL>
    ;
```

**Последовательность с паузой** (если есть `macro_wait_time` / `wait-ms` в вашей версии — смотрите [ZMK macros](https://zmk.dev/docs/keymaps/behaviors/macros)):

```dts
bindings
    = <&macro_tap &kp A>
    , <&macro_tap &kp B>
    ;
```

Коды клавиш: `<dt-bindings/zmk/keys.h>` (`LCTRL`, `LGUI`, `LALT`, `F1`…, буквы, и т.д.).

### 4. macOS

Вместо `LCTRL` часто нужен `LGUI` (Cmd):

```dts
= <&macro_press &kp LGUI>
, <&macro_tap &kp C>
, <&macro_release &kp LGUI>
;
```

### 5. Чего не делать

- Не забывайте `macro_release` для каждого `macro_press`.  
- Не оставляйте рассинхрон двух файлов keymap.  
- Длинные макросы увеличивают прошивку — держите короткими.

## Studio

ZMK Studio на left может менять биндинги слоёв, но **новые** macro-узлы всё равно задаются в keymap/DTS и требуют пересборки.
