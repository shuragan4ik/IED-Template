# Примеры работы с периферией платы. Используются только пакеты из IEDOS SDK
# (libgpiod есть в sysroot SDK и на плате).

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    # При CROSS_COMPILE=ON toolchain.cmake настроит pkg-config на sysroot SDK.
    pkg_check_modules(LIBGPIOD QUIET libgpiod)
endif()

if(NOT LIBGPIOD_FOUND)
    # Запасной путь: найти заголовок и библиотеку напрямую (в sysroot при кросс-сборке).
    find_path(LIBGPIOD_INCLUDE_DIR gpiod.h)
    find_library(LIBGPIOD_LIBRARY gpiod)
    if(LIBGPIOD_INCLUDE_DIR AND LIBGPIOD_LIBRARY)
        set(LIBGPIOD_FOUND TRUE)
    endif()
endif()

if(NOT LIBGPIOD_FOUND)
    message(WARNING
        "libgpiod не найдена — пример blink_led не будет собран.\n"
        "Для кросс-сборки укажите SDK: export IEDOS_SDK=<..._sdk-buildroot>.\n"
        "Для сборки на хосте установите libgpiod-dev или собирайте с CROSS_COMPILE=ON.")
    return()
endif()

add_executable(blink_led blink_led.c)
target_include_directories(blink_led PRIVATE ${LIBGPIOD_INCLUDE_DIRS})
target_link_libraries(blink_led PRIVATE ${LIBGPIOD_LIBRARIES})
target_compile_options(blink_led PRIVATE -Wall -Wextra -pedantic)
