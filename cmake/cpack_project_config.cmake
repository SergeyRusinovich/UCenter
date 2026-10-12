# CPack: настройки, зависящие от генератора (выполняется для каждого генератора отдельно)
if(CPACK_GENERATOR STREQUAL "WIX")
    # MSI принимает только числовую версию X.Y.Z; полная (1.2.0-3-gabc1234) остаётся в имени файла
    set(CPACK_PACKAGE_VERSION "${CPACK_UCENTER_VERSION}")
endif()
