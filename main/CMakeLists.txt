set(COMPONENT_SRCS
    "app_main.c"
    "app_settings.c"
    "app_wifi.c"
    "app_ota.c"
)

set(COMPONENT_ADD_INCLUDEDIRS
    "include"
)

set(COMPONENT_REQUIRES
    nvs_flash
    esp_http_server
)

register_component()
