# IED-Template

Шаблон проекта прошивки интеллектуального электронного устройства (ИЭУ) по стандарту **IEC 61850** на языке C. Стек протокола — [libiec61850](https://github.com/mz-automation/libiec61850), подключённый как git-сабмодуль.

Целевые платы (прошивки [iedos](../iedos), Buildroot 2026.08):

| Плата | Архитектура | Кросс-конфигурация | IP (статический) |
|---|---|---|---|
| Orange Pi PC 2 (H5) | aarch64 | `-DIEDOS_BOARD=orangepi-pc2` | 192.168.0.101 |
| Orange Pi One (H3) | arm 32-bit | `-DIEDOS_BOARD=orangepi-one` | 192.168.0.100 |

## Структура

```
├── src/main.c                  # точка входа (заполняется в рамках заданий)
├── include/                    # заголовочные файлы проекта
├── cmake/toolchain.cmake       # тулчейн кросс-компиляции (Buildroot SDK, обе платы)
└── third_party/libiec61850/    # сабмодуль: стек IEC 61850 + примеры (goose_publisher, goose_observer, ...)
```

## Требования

- Linux (Ubuntu 22.04+) или WSL2;
- git, cmake ≥ 3.10, gcc, gdb;
- **Buildroot SDK (IEDOS)** — перемещаемый архив из сборки образов `iedos`
  (`output/iedos_*/images/*_sdk-buildroot.tar.gz`);
- для реальной отправки GOOSE: права root (raw-сокеты).

Однократная подготовка SDK (для каждой платы, которую используете):

```bash
mkdir -p ~/iedos-sdk && cd ~/iedos-sdk
tar xf <путь>/aarch64-linux-gnu_sdk-buildroot.tar.gz
./aarch64-linux-gnu_sdk-buildroot/relocate-sdk.sh
export IEDOS_SDK=$HOME/iedos-sdk/aarch64-linux-gnu_sdk-buildroot   # в ~/.zshrc/.bashrc
```

> Для Orange Pi One — аналогично `arm-linux-gnueabihf_sdk-buildroot`.
> `IEDOS_SDK` должен указывать на SDK той платы, под которую собираете/отлаживаете.
> В sysroot SDK уже есть libiec61850, libgpiod, boost, gtest и другие пакеты прошивки.

## Сборка

Клонирование (важен флаг сабмодулей):

```bash
git clone --recurse-submodules <url> && cd IED-Template
```

Нативная сборка (x86_64, для локальной отладки):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/ied_exe eth0        # имя своего интерфейса; для GOOSE — sudo
```

Кросс-сборка под Orange Pi PC 2 (aarch64):

```bash
cmake -B build-aarch64 -DCMAKE_BUILD_TYPE=Debug -DCROSS_COMPILE=ON -DIEDOS_BOARD=orangepi-pc2
cmake --build build-aarch64 -j
```

Кросс-сборка под Orange Pi One (arm 32-bit):

```bash
cmake -B build-arm -DCMAKE_BUILD_TYPE=Debug -DCROSS_COMPILE=ON -DIEDOS_BOARD=orangepi-one
cmake --build build-arm -j
```

## Отладка в VS Code

Конфигурации уже подготовлены в `.vscode/`:

| Задача / конфигурация | Назначение |
|---|---|
| `Build (host x86_64)` (F7) | сборка под локальную отладку |
| `Debug (host x86_64)` (F5) | локальная отладка gdb |
| `Build (target orangepi-pc2)` | кросс-сборка под плату PC 2 |
| `Build (target orangepi-one)` | кросс-сборка под плату One |
| `Deploy & gdbserver (orangepi-pc2/one)` | загрузка бинарника на плату + gdbserver :1234 |
| `Debug (orangepi-pc2/one, gdbserver)` (F5) | удалённая отладка на плате (кросс-gdb из `$IEDOS_SDK`) |
| `QEMU + cross-gdb (terminal)` | проверка aarch64-бинарника в эмуляторе без железа |

Удалённая отладка работает «из коробки»: IP плат зашиты прошивкой
(см. таблицу выше), root без пароля (аудитория изолирована). Каталоги
`build/`, `build-aarch64/`, `build-arm/` независимы.

## Лицензия

[GPLv3](LICENSE)
