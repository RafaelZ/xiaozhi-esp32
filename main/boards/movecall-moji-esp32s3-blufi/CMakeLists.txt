list(APPEND srcs
        blufi_init.c
        blufi_security.c
        doit_blufi_init.c
        doit_blufi.c
)

list(APPEND incs
        .
)

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${incs}"
                       EMBED_TXTFILES "${assets}"
                       REQUIRES driver esp_wifi bt nvs_flash json esp_timer mbedtls) 