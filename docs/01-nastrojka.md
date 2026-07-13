# Настройка прошивки Corne v3 (ZMK)

Подробная карта файлов репозитория: **что за что отвечает** и **куда править**, чтобы изменить поведение клавиатуры.

Связанный документ: [Использование клавиатуры](02-ispolzovanie.md).

---

## 1. Карта репозитория

```
zmk-for-keys/
├── build.yaml                 # Какие прошивки собирать в GitHub Actions
├── pinout nicenano.txt        # Исходный пин-аут / параметры «железа»
├── docs/                      # Эти инструкции
├── .github/workflows/build.yml
└── config/
    ├── west.yml               # Версия ZMK (сейчас main / Zephyr 4.x)
    ├── corne_v3.conf          # Общие Kconfig для всех сборок corne_v3_*
    ├── corne_v3.keymap        # Раскладка (слои, combos, hold-tap)
    └── boards/shields/corne_v3/
        ├── corne_v3.dtsi      # Общая схема: матрица, I2C, RGB, split input
        ├── corne_v3_left.*    # Левая половина (central): OLED, слушатели трекбола
        ├── corne_v3_right.*   # Правая половина (peripheral): PAT912x
        ├── corne_v3_left_bare.* # Диагностика без OLED/RGB/pointing
        ├── custom_status_screen.c # (отключён — ломал boot; сейчас built-in экран)
        └── CMakeLists.txt
```

Сборка: push в GitHub → Actions → артефакты `corne_v3_left-*.uf2` и `corne_v3_right-*.uf2`.

| Артефакт | Куда прошивать |
|----------|----------------|
| `corne_v3_left` | Левый nice!nano (central, OLED, связь с ПК) |
| `corne_v3_right` | Правый nice!nano (peripheral, трекбол) |
| `settings_reset` | Сброс BLE-пар / настроек (обе половины по очереди) |

---

## 2. Роли половин

| | Левая (`corne_v3_left`) | Правая (`corne_v3_right`) |
|--|-------------------------|---------------------------|
| Роль ZMK | **Central** | **Peripheral** |
| Связь с ПК | USB и/или BLE | Нет (только с левой по BLE) |
| OLED | Да (SSD1306) | Нет |
| Трекбол PAT912x | Нет (принимает события) | Да |
| RGB | 27 LED на своей половине | 27 LED на своей половине |

Левая половина — «мозг»: раскладка, слои, HID. Правая только шлёт нажатия/трекбол на левую.

---

## 3. Где что настраивается

### 3.1. Пины и железо — `pinout nicenano.txt` + DTS

Исходные числа живут в `pinout nicenano.txt`. Реальная прошивка читает **devicetree**:

| Параметр | Файл | Узел / свойство |
|----------|------|-----------------|
| Строки матрицы | `corne_v3.dtsi` | `kscan0` → `row-gpios` (D4–D7) |
| Колонки левой | `corne_v3_left.overlay` | `kscan0` → `col-gpios` |
| Колонки правой | `corne_v3_right.overlay` | `kscan0` → `col-gpios` (зеркально) |
| RGB Data (D1 / P0.06) | `corne_v3.dtsi` | `&spi3` / `led_strip`, `chain-length = <27>` |
| I2C OLED/трекбол (D2/D3) | `corne_v3.dtsi` | `&pinctrl` i2c0, `&pro_micro_i2c` |
| OLED 128×32 addr 0x3C | `corne_v3.dtsi` | `oled: ssd1306@3c` (включается на left) |
| PAT912x addr 0x75 | `corne_v3_right.overlay` | датчик + motion GPIO + кнопка |
| Масштаб трекбола ÷2 | `corne_v3_left.overlay` | `zip_xy_scaler 1 2` |
| Скролл трекбола на слое 2 | `corne_v3_left.overlay` | `scroll { layers = <2>; ... }` |

Меняете пин → правьте **overlay/dtsi**, не только `pinout nicenano.txt` (тот файл — справочник).

### 3.2. Общие флаги — `config/corne_v3.conf` и `corne_v3.conf` в щите

| Опция | Смысл |
|-------|--------|
| `CONFIG_ZMK_KSCAN_DEBOUNCE_*_MS` | Антидребезг (сейчас **5 ms**) |
| `CONFIG_ZMK_KEYBOARD_NAME` | Имя BLE-устройства («Corne») |
| `CONFIG_BT_CTLR_TX_PWR_PLUS_8` | Мощность BLE |
| `CONFIG_ZMK_USB` / `CONFIG_ZMK_BLE` | Транспорты к ПК |
| `CONFIG_ZMK_SLEEP` | Глубокий сон (сейчас выключен для отладки) |
| `CONFIG_ZMK_STUDIO` | ZMK Studio по USB (сейчас **выключен**) |

Боковые файлы:

- `corne_v3_left.conf` — OLED, RGB, pointing, кастомный экран
- `corne_v3_right.conf` — RGB, pointing, **без** display

### 3.3. Раскладка — `config/corne_v3.keymap`

Здесь:

- слои (`default` / `lower` / `raise` / `ADJ`);
- `display-name` для OLED (`DEF`, `LOW`, `RAI`, `ADJ`);
- hold-tap (`hm`, `lt`, `ltq`);
- combos (скобки и т.п.);
- `&bt`, `&out`, `&rgb_ug`, `&bootloader`.

Дубликат в `boards/shields/corne_v3/corne_v3.keymap` должен совпадать с корневым `config/corne_v3.keymap` (удобно править **оба** или синхронизировать).

Как читать bindings Corne 3×6+3:

```
ряд 1: 6 клавиш слева | 6 справа
ряд 2: 6 слева        | 6 справа
ряд 3: 6 слева        | 6 справа
ряд 4:     3 слева    | 3 справа
```

### 3.4. RGB

| Что | Где |
|-----|-----|
| Число LED | `corne_v3.dtsi` → `chain-length = <27>` (6 underglow + 21 per-key) |
| Драйв MOSI | `nordic,drive-mode = <NRF_DRIVE_H0H1>` |
| Вкл. эффект / яркость | `corne_v3_left.conf` / `_right.conf` → `CONFIG_ZMK_RGB_UNDERGLOW_*` |
| Переключение с клавиатуры | слой LOW → `&rgb_ug RGB_TOG` |

Если горят только первые N LED — обычно обрыв цепи после N-го (пайка DO→DI), не `chain-length`.

### 3.5. OLED

| Что | Где |
|-----|-----|
| Драйвер / размер | `corne_v3.dtsi` → `ssd1306@3c` |
| Кастомный экран | `custom_status_screen.c` |
| Включение | `corne_v3_left.conf` → `CONFIG_ZMK_DISPLAY=y`, `…_STATUS_SCREEN_CUSTOM=y` |

На экране (128×32):

| Угол | Содержание |
|------|------------|
| Верх слева | Выход на ПК: USB / BLE профиль |
| Верх справа | Заряд левой (%) |
| Низ слева | **`R:OK`** = правая подключена, **`R:--`** = нет |
| Низ справа | **`L0 DEF`** — номер и имя активного слоя |

### 3.6. Split / BLE между половинами

Включается автоматически через `Kconfig.defconfig` (`CONFIG_ZMK_SPLIT`, left = central).

Первое сопряжение половин:

1. Прошить обе стороны одной сборкой.
2. Если не видят друг друга — прошить `settings_reset` на **обе**, затем снова left/right.
3. Правая должна быть под питанием (USB или батарея), даже вне PCB.

Правая **не** появляется в списке Bluetooth ПК — это нормально.

### 3.7. Версия ZMK — `config/west.yml`

```yaml
revision: main   # Zephyr 4.x, нативный PAT912x
```

Менять осторожно: от `revision` зависят драйверы (RGB, input, split).

### 3.8. Матрица сборок — `build.yaml`

Список UF2 в Actions. Для повседневной работы нужны `corne_v3_left` и `corne_v3_right`. Остальное — диагностика (`settings_reset`, `tester_*`, `*_bare`).

---

## 4. Типовые правки «куда идти»

| Хочу… | Файл |
|-------|------|
| Поменять буквы / слои | `config/corne_v3.keymap` |
| Имена слоёв на OLED | `display-name` в keymap |
| Быстрее/медленнее антидребезг | `CONFIG_ZMK_KSCAN_DEBOUNCE_*` в `.conf` |
| Hold-tap «вязкий» на A/S/D/F | `tapping-term-ms` у `hm` / `lt` / `ltq` в keymap |
| CPI / инверсия трекбола | right overlay (датчик) + left `zip_xy_scaler` |
| Скролл трекбола на другом слое | `layers = <N>` в `corne_v3_left.overlay` |
| Число RGB | `chain-length` в `corne_v3.dtsi` |
| Текст/логика OLED | `custom_status_screen.c` |
| Имя в Bluetooth | `CONFIG_ZMK_KEYBOARD_NAME` |
| Включить ZMK Studio | `CONFIG_ZMK_STUDIO=y` (left) + пересборка |

---

## 5. Как применить изменение

1. Правите файлы → `git commit` / push (или PR).
2. Дождитесь зелёного **Build ZMK firmware** в Actions.
3. Скачайте `firmware.zip` → нужный `.uf2`.
4. Дважды RESET на nice!nano → копируете UF2 на диск `NICENANO`.
5. После смены BLE/split настроек иногда нужен `settings_reset`.

Подробнее про ежедневное использование, OLED-флаги и софт для слоёв — в [02-ispolzovanie.md](02-ispolzovanie.md).
