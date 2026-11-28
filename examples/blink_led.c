// Пример: мигание светодиодом на gpiochip1, линия 11 (Orange Pi, IEDOS).
//
// Используется только SDK (IEDOS Buildroot SDK): в sysroot уже есть
// libgpiod v2 (заголовок <gpiod.h>, библиотека -lgpiod) — внешних
// зависимостей нет.
//
// Сборка (кросс, из корня репозитория):
//   cmake --preset orangepi-pc2 && cmake --build --preset orangepi-pc2 -j
//   # бинарник: build-aarch64/examples/blink_led
//
// Запуск на плате (нужны права root или доступ к /dev/gpiochip*):
//   ./blink_led            # мигает 1 Гц (500 мс включено / 500 мс выключено)
//   Ctrl+C — корректное завершение (линия освобождается).

// nanosleep() и signal() видимы в glibc при строгом -std=c11 только с этим макросом.
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <gpiod.h>

// Параметры светодиода по условию примера.
static const char kChipPath[] = "/dev/gpiochip1";
static const unsigned int kLineOffset = 11;
static const unsigned int kPeriodMs = 500;  // полупериод мигания

static volatile sig_atomic_t g_running = 1;

static void on_signal(int sig) {
  (void)sig;
  g_running = 0;
}

int main(void) {
  struct gpiod_chip *chip = NULL;
  struct gpiod_line_settings *settings = NULL;
  struct gpiod_line_config *line_cfg = NULL;
  struct gpiod_request_config *req_cfg = NULL;
  struct gpiod_line_request *request = NULL;
  int status = EXIT_FAILURE;

  // 1. Открываем чип (в SDK-версии libgpiod есть только gpiod_chip_open по пути).
  chip = gpiod_chip_open(kChipPath);
  if (!chip) {
    fprintf(stderr, "Не удалось открыть %s: %s\n", kChipPath, strerror(errno));
    goto cleanup;
  }

  // 2. Настраиваем линию 11 как выход с начальным уровнем «выключено».
  settings = gpiod_line_settings_new();
  if (!settings) {
    fprintf(stderr, "gpiod_line_settings_new: %s\n", strerror(errno));
    goto cleanup;
  }
  if (gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT) != 0) {
    fprintf(stderr, "gpiod_line_settings_set_direction: %s\n", strerror(errno));
    goto cleanup;
  }
  if (gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE) != 0) {
    fprintf(stderr, "gpiod_line_settings_set_output_value: %s\n", strerror(errno));
    goto cleanup;
  }

  line_cfg = gpiod_line_config_new();
  if (!line_cfg) {
    fprintf(stderr, "gpiod_line_config_new: %s\n", strerror(errno));
    goto cleanup;
  }
  if (gpiod_line_config_add_line_settings(line_cfg, &kLineOffset, 1, settings) != 0) {
    fprintf(stderr, "gpiod_line_config_add_line_settings: %s\n", strerror(errno));
    goto cleanup;
  }

  req_cfg = gpiod_request_config_new();
  if (!req_cfg) {
    fprintf(stderr, "gpiod_request_config_new: %s\n", strerror(errno));
    goto cleanup;
  }
  gpiod_request_config_set_consumer(req_cfg, "blink_led");  // имя потребителя видно в gpioinfo

  // 3. Запрашиваем линию — с этого момента она принадлежит нам.
  request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);
  if (!request) {
    fprintf(stderr, "gpiod_chip_request_lines (%s, линия %u): %s\n", kChipPath,
            kLineOffset, strerror(errno));
    goto cleanup;
  }

  // 4. Ctrl+C / SIGTERM корректно останавливают цикл.
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = on_signal;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  // 5. Мигание: переключаем уровень каждые kPeriodMs миллисекунд.
  printf("Мигание светодиодом: %s, линия %u, период %u мс (Ctrl+C — выход)\n",
         kChipPath, kLineOffset, kPeriodMs);

  enum gpiod_line_value value = GPIOD_LINE_VALUE_INACTIVE;
  while (g_running) {
    value = (value == GPIOD_LINE_VALUE_ACTIVE) ? GPIOD_LINE_VALUE_INACTIVE
                                               : GPIOD_LINE_VALUE_ACTIVE;
    if (gpiod_line_request_set_value(request, kLineOffset, value) != 0) {
      fprintf(stderr, "gpiod_line_request_set_value: %s\n", strerror(errno));
      goto cleanup;
    }

    const struct timespec ts = {
        .tv_sec = kPeriodMs / 1000,
        .tv_nsec = (long)(kPeriodMs % 1000) * 1000000L,
    };
    nanosleep(&ts, NULL);  // прерывается сигналом — цикл проверит g_running
  }

  // 6. Гасим светодиод перед выходом.
  if (gpiod_line_request_set_value(request, kLineOffset, GPIOD_LINE_VALUE_INACTIVE) != 0) {
    fprintf(stderr, "gpiod_line_request_set_value: %s\n", strerror(errno));
  }
  status = EXIT_SUCCESS;

cleanup:
  // Порядок важен: сначала освобощаем запрос линии, затем закрываем чип.
  gpiod_line_request_release(request);
  gpiod_request_config_free(req_cfg);
  gpiod_line_config_free(line_cfg);
  gpiod_line_settings_free(settings);
  gpiod_chip_close(chip);
  return status;
}
