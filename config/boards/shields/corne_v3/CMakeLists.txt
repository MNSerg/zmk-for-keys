# Copyright (c) 2026 The ZMK Contributors
# SPDX-License-Identifier: MIT

# Must land in the `app` target so the strong zmk_display_status_screen()
# overrides the weak stub, and ZMK event sections link correctly.
# (zephyr_library_sources can leave the weak NULL screen in place → dead OLED.)
if(CONFIG_ZMK_DISPLAY AND CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM)
  target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/custom_status_screen.c)
endif()
