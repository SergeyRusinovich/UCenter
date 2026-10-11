cmake_minimum_required(VERSION 3.21)

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

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Positioning SerialPort Test)
qt_standard_project_setup()

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

# ---------- Тесты ----------
enable_testing()
qt_add_executable(test_core tests/test_core.cpp)
target_link_libraries(test_core PRIVATE ucenter_core Qt6::Test)
ucenter_warnings(test_core)
add_test(NAME test_core COMMAND test_core)

if(UNIX)
    # Сквозной тест порта через псевдотерминал (openpty): только macOS и Linux
    qt_add_executable(test_session tests/test_session.cpp)
    target_link_libraries(test_session PRIVATE ucenter_core Qt6::Test $<$<PLATFORM_ID:Linux>:util>)
    ucenter_warnings(test_session)
    add_test(NAME test_session COMMAND test_session)
endif()

if(UCENTER_FUZZ)
    add_executable(fuzz_frame_reader fuzz/fuzz_frame_reader.cpp)
    target_link_libraries(fuzz_frame_reader PRIVATE ucenter_core)
    target_link_options(fuzz_frame_reader PRIVATE -fsanitize=fuzzer)
endif()
