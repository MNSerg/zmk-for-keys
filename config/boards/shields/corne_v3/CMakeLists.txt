# Copyright (c) 2026 The ZMK Contributors
# SPDX-License-Identifier: MIT

if(CONFIG_ZMK_DISPLAY)
  zephyr_library_sources(custom_status_screen.c)
endif()
