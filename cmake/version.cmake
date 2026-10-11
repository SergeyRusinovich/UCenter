# Версия из git: тег вида v1.2.3 задаёт номер, остальное (коммиты после тега, хеш, правки) —
# полную строку для показа. Без тега — UCENTER_BASE_VERSION и хеш коммита.
# Определяется при настройке CMake: после нового тега нужно перенастроить сборку.
set(UCENTER_BASE_VERSION 1.0.0)

set(UCENTER_VERSION ${UCENTER_BASE_VERSION})
set(UCENTER_VERSION_FULL ${UCENTER_BASE_VERSION})

find_program(GIT_EXECUTABLE git)
if(GIT_EXECUTABLE)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --match "v[0-9]*" --dirty --always
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
        OUTPUT_VARIABLE _describe
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _result)
    if(_result EQUAL 0 AND _describe)
        if(_describe MATCHES "^v([0-9]+\\.[0-9]+(\\.[0-9]+)?)(.*)$")
            set(UCENTER_VERSION ${CMAKE_MATCH_1})
            set(UCENTER_VERSION_FULL ${CMAKE_MATCH_1}${CMAKE_MATCH_3})
        else()
            # Тегов нет: describe вернул только хеш (и -dirty)
            set(UCENTER_VERSION_FULL ${UCENTER_BASE_VERSION}+g${_describe})
        endif()
    endif()
endif()
