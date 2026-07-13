# Copyright (c) 2026 The ZMK Contributors
# SPDX-License-Identifier: MIT
#
# Same pattern as ZMK's own status_screen.c: must be part of `app`,
# otherwise the weak stub wins and/or event sections break.

if(CONFIG_ZMK_DISPLAY AND CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM)
  target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/custom_status_screen.c)
endif()
