cmake_minimum_required(VERSION 3.22)

include(cmake/version.cmake)
project(UCenter VERSION ${UCENTER_VERSION} DESCRIPTION "Desktop GNSS viewer for u-blox M9" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# compile_commands.json: пути к заголовкам Qt для IntelliSense, clangd и clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(UCENTER_WERROR "Treat compiler warnings as errors" OFF)
option(UCENTER_FUZZ "Build the libFuzzer target for the frame reader (clang only)" OFF)
# ASan из LLVM 19 зависает при старте на macOS 26 — там фаззер собирают с undefined
set(UCENTER_FUZZ_SANITIZERS "address,undefined" CACHE STRING "Sanitizers for the fuzz build")

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Widgets Network Positioning SerialPort Svg Test LinguistTools)
qt_standard_project_setup(I18N_SOURCE_LANGUAGE en I18N_TRANSLATED_LANGUAGES ru es zh_CN)

# Предупреждения одинаковые для всех целей проекта (сгенерированный Qt код их не получает)
function(ucenter_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- $<$<BOOL:${UCENTER_WERROR}>:/WX>)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow
                               $<$<BOOL:${UCENTER_WERROR}>:-Werror>)
    endif()
endfunction()

if(UCENTER_FUZZ)
    # Покрытие для libFuzzer и санитайзеры — во всём коде, который он вызывает
    add_compile_options(-fsanitize=fuzzer-no-link,${UCENTER_FUZZ_SANITIZERS} -fno-omit-frame-pointer)
    add_link_options(-fsanitize=${UCENTER_FUZZ_SANITIZERS})
endif()

# ---------- Ядро: без виджетов (QtCore, QtPositioning, QtSerialPort), покрыто тестами ----------
add_library(ucenter_core STATIC
    src/core/baseline.cpp
    src/core/baseline_store.cpp
    src/core/frame_reader.cpp
    src/core/geo.cpp
    src/core/host_location.cpp
    src/core/language.cpp
    src/core/messages.cpp
    src/core/navigation.cpp
    src/core/nmea.cpp
    src/core/places.cpp
    src/core/points.cpp
    src/core/ports.cpp
    src/core/receiver_config.cpp
    src/core/replay.cpp
    src/core/satellites.cpp
    src/core/session.cpp
    src/core/source.cpp
    src/core/timeutil.cpp
    src/core/ubx.cpp
    src/core/worker.cpp
)
target_include_directories(ucenter_core PUBLIC src)
target_link_libraries(ucenter_core PUBLIC Qt6::Core Qt6::Positioning Qt6::SerialPort)
ucenter_warnings(ucenter_core)

# ---------- Приложение ----------
set(UCENTER_ICNS ${PROJECT_SOURCE_DIR}/resources/icons/ucenter.icns)
set_source_files_properties(${UCENTER_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

qt_add_executable(UCenter WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/ui/azimuth_tab.cpp
    src/ui/baseline_map_view.cpp
    src/ui/compass_view.cpp
    src/ui/config_tab.cpp
    src/ui/connection_panel.cpp
    src/ui/console_tab.cpp
    src/ui/dashboard_tab.cpp
    src/ui/filters.cpp
    src/ui/geocoder.cpp
    src/ui/imagery_picker.cpp
    src/ui/location_popover.cpp
    src/ui/main_window.cpp
    src/ui/map_buttons.cpp
    src/ui/map_view.cpp
    src/ui/measurement_panels.cpp
    src/ui/network.cpp
    src/ui/painting.cpp
    src/ui/place_search_bar.cpp
    src/ui/satellite_views.cpp
    src/ui/satellites_tab.cpp
    src/ui/saved_measurements.cpp
    src/ui/style.cpp
    src/ui/theme.cpp
    src/ui/tile_loader.cpp
    $<$<PLATFORM_ID:Darwin>:${UCENTER_ICNS}>
    $<$<PLATFORM_ID:Windows>:${PROJECT_SOURCE_DIR}/resources/ucenter.rc>
)
target_compile_definitions(UCenter PRIVATE UCENTER_VERSION="${UCENTER_VERSION_FULL}")
qt_add_resources(UCenter assets
    PREFIX /
    BASE src/ui
    FILES src/ui/assets/check.svg src/ui/assets/chevron-dark.svg src/ui/assets/chevron-light.svg
)
qt_add_resources(UCenter icons
    PREFIX /icons
    BASE resources/icons
    FILES resources/icons/ucenter.png
)
# Svg — для значков в QSS (плагин imageformats/qsvg)
target_link_libraries(UCenter PRIVATE ucenter_core Qt6::Widgets Qt6::Network Qt6::Svg)
ucenter_warnings(UCenter)

# Переводы: исходные строки — английские, остальные языки — translations/ucenter_<язык>.ts.
# Обновить .ts после правок строк: cmake --build build --target update_translations
set(UCENTER_LANGUAGES ru es zh_CN)
list(TRANSFORM UCENTER_LANGUAGES PREPEND translations/ucenter_ OUTPUT_VARIABLE UCENTER_TS_FILES)
list(TRANSFORM UCENTER_TS_FILES APPEND .ts)
qt_add_translations(TARGETS UCenter
    SOURCE_TARGETS UCenter ucenter_core
    TS_FILES ${UCENTER_TS_FILES}
    RESOURCE_PREFIX /i18n
    LUPDATE_OPTIONS -no-obsolete -locations none)
# Перевод стандартных диалогов Qt (кнопки, выбор файла) — внутрь приложения, чтобы не зависеть от установки Qt
set(QT_TRANSLATIONS_DIR "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_TRANSLATIONS}")
set(UCENTER_QTBASE_QM)
foreach(lang IN LISTS UCENTER_LANGUAGES)
    if(EXISTS "${QT_TRANSLATIONS_DIR}/qtbase_${lang}.qm")
        list(APPEND UCENTER_QTBASE_QM "${QT_TRANSLATIONS_DIR}/qtbase_${lang}.qm")
    else()
        message(WARNING "qtbase_${lang}.qm not found: standard Qt dialogs stay in English for ${lang}")
    endif()
endforeach()
if(UCENTER_QTBASE_QM)
    qt_add_resources(UCenter qt_translations PREFIX /i18n BASE "${QT_TRANSLATIONS_DIR}" FILES ${UCENTER_QTBASE_QM})
endif()

set_target_properties(UCenter PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/cmake/Info.plist.in
    MACOSX_BUNDLE_GUI_IDENTIFIER local.ucenter
    MACOSX_BUNDLE_BUNDLE_NAME UCenter
    MACOSX_BUNDLE_ICON_FILE ucenter.icns
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${UCENTER_VERSION_FULL}
)
if(APPLE)
    # macOS выдаёт геопозицию только подписанному бандлу с NSLocationUsageDescription в Info.plist;
    # ad-hoc подпись всего бандла нужна, чтобы macOS запомнила разрешение
    add_custom_command(TARGET UCenter POST_BUILD
        COMMAND codesign --force --sign - $<TARGET_BUNDLE_DIR:UCenter>
        VERBATIM)
endif()

# ---------- Установка и пакеты: Qt кладётся рядом с приложением ----------
include(GNUInstallDirs)
install(TARGETS UCenter
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
qt_generate_deploy_app_script(TARGET UCenter OUTPUT_SCRIPT deploy_script NO_UNSUPPORTED_PLATFORM_ERROR)
install(SCRIPT ${deploy_script})
if(APPLE)
    # macdeployqt меняет файлы внутри бандла — подпись нужно обновить
    install(CODE "execute_process(COMMAND codesign --force --deep --sign - \"\${CMAKE_INSTALL_PREFIX}/UCenter.app\")")
endif()

set(CPACK_PACKAGE_NAME UCenter)
set(CPACK_PACKAGE_VENDOR UCenter)
set(CPACK_PACKAGE_VERSION ${UCENTER_VERSION_FULL})
set(CPACK_PACKAGE_FILE_NAME UCenter-${UCENTER_VERSION_FULL}-${CMAKE_SYSTEM_NAME})
if(APPLE)
    set(CPACK_GENERATOR DragNDrop)
    set(CPACK_DMG_VOLUME_NAME UCenter)
elseif(WIN32)
    set(CPACK_GENERATOR ZIP)
else()
    set(CPACK_GENERATOR TGZ)
endif()
include(CPack)

# ---------- Тесты ----------
enable_testing()
# Эталонная запись для замеров (дорожная карта, фаза 1): генератор, а не файл в репозитории
add_library(ucenter_recording STATIC tests/recording.cpp)
target_link_libraries(ucenter_recording PUBLIC ucenter_core)
ucenter_warnings(ucenter_recording)

qt_add_executable(test_core tests/test_core.cpp)
target_link_libraries(test_core PRIVATE ucenter_core ucenter_recording Qt6::Test)
ucenter_warnings(test_core)
add_test(NAME test_core COMMAND test_core)

# Замеры ядра и файл эталонной записи для приложения; в ctest не входят — запускаются вручную
qt_add_executable(bench_core tests/bench_core.cpp)
target_link_libraries(bench_core PRIVATE ucenter_recording Qt6::Test)
ucenter_warnings(bench_core)
qt_add_executable(make_recording tests/make_recording.cpp)
target_link_libraries(make_recording PRIVATE ucenter_recording)
ucenter_warnings(make_recording)

# Все строки всех языков переведены (нет «unfinished» в .ts)
add_test(NAME translations_complete
    COMMAND ${CMAKE_COMMAND} -P ${PROJECT_SOURCE_DIR}/cmake/check_translations.cmake
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

if(UNIX)
    # Сквозной тест порта через псевдотерминал (openpty): только macOS и Linux
    qt_add_executable(test_session tests/test_session.cpp)
    target_link_libraries(test_session PRIVATE ucenter_core Qt6::Test $<$<PLATFORM_ID:Linux>:util>)
    ucenter_warnings(test_session)
    add_test(NAME test_session COMMAND test_session)
endif()

if(WIN32)
    # Рядом с тестами нет Qt (его кладёт только windeployqt при установке): DLL ищутся через PATH
    set_tests_properties(test_core PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
endif()

if(UCENTER_FUZZ)
    add_executable(fuzz_frame_reader fuzz/fuzz_frame_reader.cpp)
    target_link_libraries(fuzz_frame_reader PRIVATE ucenter_core)
    target_link_options(fuzz_frame_reader PRIVATE -fsanitize=fuzzer)
endif()
