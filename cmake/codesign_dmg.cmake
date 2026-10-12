# CPack, после создания пакетов: подпись .dmg тем же Developer ID (ADR 0019).
# С ad hoc подписью образ не подписывается: Gatekeeper ad hoc подпись образа всё равно не принимает.
if(CPACK_UCENTER_CODESIGN_IDENTITY STREQUAL "-" OR NOT CPACK_UCENTER_CODESIGN_IDENTITY)
    return()
endif()
foreach(package IN LISTS CPACK_PACKAGE_FILES)
    if(NOT package MATCHES "\\.dmg$")
        continue()
    endif()
    # Сервер меток времени Apple иногда отклоняет запрос: до трёх попыток
    foreach(attempt RANGE 1 3)
        execute_process(COMMAND codesign --force --timestamp --sign "${CPACK_UCENTER_CODESIGN_IDENTITY}" "${package}"
                        RESULT_VARIABLE result)
        if(result EQUAL 0)
            break()
        endif()
        execute_process(COMMAND ${CMAKE_COMMAND} -E sleep 5)
    endforeach()
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "codesign failed for ${package}")
    endif()
endforeach()
