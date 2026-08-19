# Idle и сон (RGB / deep sleep)

## Поведение

| Режим | Таймаут | Что гаснет |
|-------|---------|------------|
| **Idle** | **10 с** без нажатий | RGB (и OLED уходит в low-power путь ZMK) |
| **Deep sleep** | **60 мин** бездействия (без USB) | MCU `sys_poweroff`, нужен wake с **той же** половины |

Таймауты:

- `config/corne_v3.conf` и `config/boards/shields/corne_v3/corne_v3.conf`
  - `CONFIG_ZMK_IDLE_TIMEOUT=10000`
  - `CONFIG_ZMK_SLEEP=y`
  - `CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=3600000`

RGB idle: `CONFIG_ZMK_RGB_UNDERGLOW_AUTO_OFF_IDLE=y` в `corne_v3_left.conf` / `corne_v3_right.conf`.

## Sync левая → правая

В stock ZMK активность **правой** не обновляется от набора **слева**. Реализация: `&st_sync` в `status_sync.c`.

1. Левая (central) шлёт флаг `ACTIVE` в `param1`, пока она в `ZMK_ACTIVITY_ACTIVE`.
2. Правая (peripheral) при `ACTIVE`:
   - poke виртуального input-устройства → сброс своего idle-таймера;
   - при **входе** в ACTIVE после sync-dim — `zmk_rgb_underglow_on()` (иначе после первого idle RGB справа мог остаться выкл.).
3. Когда левая уходит в IDLE (флаг снимается):
   - правая **сразу** гасит RGB (`sync_dimmed_rgb`), чтобы половины тухли вместе;
   - poke прекращается.

Poke **никогда** не делается на левой — иначе левая не уйдёт в idle за 10 с.

Файлы:

| Файл | Роль |
|------|------|
| `status_sync.c` / `.h` | ACTIVE flag, poke, RGB dim/restore |
| `corne_v3.keymap` | узел `st_sync` |
| `CMakeLists.txt` | всегда компилирует `status_sync.c` |

Правая→левая: штатно (клавиши/трекбол с peripheral будят central).

## Отличия idle vs RGB выкл.

| | Idle | `RGB_TOG` off |
|--|------|----------------|
| После нажатия | RGB снова (если был вкл.) | остаётся выкл. |
| Settings | on сохраняется | off пишется в flash |

## Deep sleep

Пробуждение — клавиша на **этой** половине (или RESET после `&soft_off`). Трекбол не wakeup-source. Пока печатаете слева, правая не должна уходить в deep sleep одна (ACTIVE poke).

## Настройка

Увеличить idle до 30 с: `CONFIG_ZMK_IDLE_TIMEOUT=30000` в обоих `corne_v3.conf`. Период `st_sync` (~4 с) должен быть **меньше** половины idle.
