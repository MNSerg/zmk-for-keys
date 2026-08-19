# Split и BLE

## Роли

| Половина | Роль |
|----------|------|
| Left | Central — HID к ПК (USB/BLE), раскладка |
| Right | Peripheral — только к left по BLE |

`Kconfig.defconfig`: `ZMK_SPLIT_ROLE_CENTRAL` на left.

## Имя устройства

`CONFIG_ZMK_KEYBOARD_NAME` в `corne_v3.conf` (сейчас «Corne» / см. последний коммит имени).

## Сопряжение / сброс

1. Прошить `settings_reset` на **обе** половины.  
2. Снова `corne_v3_left` + `corne_v3_right` из одного zip.  
3. Заново спарить BLE на ПК (мышь+клавиатура).

OLED: `R:OK` = правая на связи, `R:--` = нет.

## ZMK Studio

Только left (полная сборка). ADJ → `&studio_unlock` → [zmk.studio](https://zmk.studio/).  
Studio-keymap может ломать правые позиции — для отладки split есть `left_nostudio` / `*_bare`.

## Файлы

| Файл | Что |
|------|-----|
| `corne_v3.dtsi` | split, kscan, chosen |
| `west.yml` | revision ZMK (`main`) |
| `build.yaml` | список shields |
