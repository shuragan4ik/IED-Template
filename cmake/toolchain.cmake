# Тулчейн кросс-компиляции под учебные платы IEDOS (Buildroot SDK).
#
# SDK — перемещаемый архив из репозитория iedos (см. README iedos):
#   tar xf <triple>_sdk-buildroot.tar.gz
#   ./<triple>_sdk-buildroot/relocate-sdk.sh
#   export IEDOS_SDK=/path/to/<triple>_sdk-buildroot
#
# Использование (через опцию CROSS_COMPILE в CMakeLists.txt):
#   cmake -B build-aarch64 -DCMAKE_BUILD_TYPE=Debug -DCROSS_COMPILE=ON \
#         -DIEDOS_BOARD=orangepi-pc2     # H5, aarch64, IP 192.168.0.101
#   cmake -B build-arm -DCMAKE_BUILD_TYPE=Debug -DCROSS_COMPILE=ON \
#         -DIEDOS_BOARD=orangepi-one     # H3, arm 32-bit, IP 192.168.0.100
#
# Каталог SDK: -DTOOLCHAIN_PATH=<путь к sdk-buildroot>, иначе $IEDOS_SDK,
# иначе /opt/iedos-sdk/<triple>_sdk-buildroot.

set(CMAKE_SYSTEM_NAME Linux)

if(NOT IEDOS_BOARD)
    set(IEDOS_BOARD "orangepi-pc2")
endif()

if(IEDOS_BOARD STREQUAL "orangepi-pc2")
    set(IEDOS_TRIPLE "aarch64-buildroot-linux-gnu")
    set(CMAKE_SYSTEM_PROCESSOR aarch64)
elseif(IEDOS_BOARD STREQUAL "orangepi-one")
    set(IEDOS_TRIPLE "arm-buildroot-linux-gnueabihf")
    set(CMAKE_SYSTEM_PROCESSOR arm)
else()
    message(FATAL_ERROR "IEDOS_BOARD должен быть orangepi-pc2 или orangepi-one")
endif()

if(NOT TOOLCHAIN_PATH AND DEFINED ENV{IEDOS_SDK})
    set(TOOLCHAIN_PATH "$ENV{IEDOS_SDK}")
endif()
if(NOT TOOLCHAIN_PATH)
    set(TOOLCHAIN_PATH "/opt/iedos-sdk/${IEDOS_TRIPLE}_sdk-buildroot")
endif()

set(CMAKE_C_COMPILER   "${TOOLCHAIN_PATH}/bin/${IEDOS_TRIPLE}-gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_PATH}/bin/${IEDOS_TRIPLE}-g++")

# Sysroot SDK совпадает с sysroot на плате: там уже есть libiec61850,
# libgpiod, boost, gtest и т.д. — find_library/find_package ищут в нём.
set(CMAKE_SYSROOT "${TOOLCHAIN_PATH}/${IEDOS_TRIPLE}/sysroot")
set(CMAKE_FIND_ROOT_PATH "${TOOLCHAIN_PATH}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# pkg-config должен искать .pc-файлы в sysroot платы, а не хостные
# (как в toolchainfile.cmake Buildroot): PKG_CONFIG_LIBDIR задаёт список
# путей поиска целиком, PKG_CONFIG_SYSROOT_DIR переписывает пути в флагах.
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")
set(ENV{PKG_CONFIG_LIBDIR}
    "${CMAKE_SYSROOT}/usr/lib/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")

if(NOT EXISTS "${CMAKE_C_COMPILER}")
    message(FATAL_ERROR
        "Компилятор ${CMAKE_C_COMPILER} не найден.\n"
        "Распакуйте SDK, выполните relocate-sdk.sh и укажите его через\n"
        "IEDOS_SDK или -DTOOLCHAIN_PATH=<путь к *_sdk-buildroot>.")
endif()

message(STATUS "IEDOS board: ${IEDOS_BOARD} (${IEDOS_TRIPLE})")
message(STATUS "IEDOS toolchain: ${TOOLCHAIN_PATH}")
message(STATUS "IEDOS sysroot:   ${CMAKE_SYSROOT}")
