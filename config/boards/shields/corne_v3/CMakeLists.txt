# Copyright (c) 2026 The ZMK Contributors
# SPDX-License-Identifier: MIT
#
# Same pattern as ZMK's own status_screen.c: must be part of `app`,
# otherwise the weak stub wins and/or event sections break.

if(CONFIG_ZMK_DISPLAY AND CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM)
  target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/custom_status_screen.c)
endif()

# Right-half PAT912x polling driver (MOTION optional, multi-address probe)
if(CONFIG_SHIELD_CORNE_V3_RIGHT AND CONFIG_ZMK_POINTING AND CONFIG_I2C)
  target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/pat912x_poll.c)
endif()

# Middle-row battery bar (&batt_bar); no-ops if strip/underglow unavailable
target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/batt_bar.c)
