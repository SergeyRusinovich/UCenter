# Подпись установленного UCenter.app (после macdeployqt, который меняет файлы бандла). ADR 0019.
# Переменные задаёт install(CODE) в CMakeLists.txt: UCENTER_CODESIGN_IDENTITY, UCENTER_ENTITLEMENTS.
set(app "${CMAKE_INSTALL_PREFIX}/UCenter.app")

# Сервер меток времени Apple иногда отклоняет запрос («A timestamp transaction was rejected»): до трёх попыток
function(ucenter_codesign)
    foreach(attempt RANGE 1 3)
        execute_process(COMMAND codesign ${ARGN} "${app}" RESULT_VARIABLE result)
        if(result EQUAL 0)
            return()
        endif()
        execute_process(COMMAND ${CMAKE_COMMAND} -E sleep 5)
    endforeach()
    message(FATAL_ERROR "codesign ${ARGN} failed for ${app}")
endfunction()

if(UCENTER_CODESIGN_IDENTITY STREQUAL "-")
    # Ad hoc: macOS помнит разрешение на геопозицию, но на других Mac Gatekeeper предупреждает
    ucenter_codesign(--force --deep --sign -)
else()
    # Developer ID — так, чтобы пакет можно было нотаризовать: hardened runtime и метка времени Apple.
    # Сначала всё вложенное (Qt, плагины), затем сам бандл — уже с правами из entitlements
    ucenter_codesign(--force --deep --options runtime --timestamp --sign "${UCENTER_CODESIGN_IDENTITY}")
    ucenter_codesign(--force --options runtime --timestamp --entitlements "${UCENTER_ENTITLEMENTS}"
                     --sign "${UCENTER_CODESIGN_IDENTITY}")
endif()
ucenter_codesign(--verify --deep --strict)
