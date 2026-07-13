# Compile the custom OLED status screen only when it's selected (corne).
if(CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM)
  target_sources(app PRIVATE src/custom_status_screen.c)
endif()
