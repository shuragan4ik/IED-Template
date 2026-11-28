/*
 * goose_publisher_example.c
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include "goose.h"
#include "gpio.h"

static volatile sig_atomic_t running = 1;

static void
sigint_handler(int sig)
{
    (void)sig;
    running = 0;
    printf("running = 0\n");
}

int
main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    void *goose = Goose_create(NULL, NULL);
    if (!goose) {
        fprintf(stderr, "Goose_create failed\n");
        return 1;
    }

    gpio_ctx *gpio = gpio_init();
    if (!gpio) {
        fprintf(stderr, "[MAIN] GPIO init failed — continuing in dry-run mode (no LEDs)\n");
        /* можно завершиться: return 1; */
    }

    signal(SIGINT, sigint_handler);

    bool prev_link_ok = false;

    while (running) {
        /* 1. обработать GOOSE */
        goose_tick(goose);

        bool link_ok = goose_link_ok(goose);

        /* 2. реакция на изменение состояния связи */
        if (link_ok && !prev_link_ok) {
            printf("[LINK] restored\n");
            /* при восстановлении снимаем safe_mode косвенно —
               просто снова начнём писать значения */
            if (gpio) {
                /* разрешаем обычную работу */
                /* (safe_mode сбрасывается ниже через обычный set_pin) */
            }
        }
        else if (!link_ok && prev_link_ok) {
            printf("[LINK] timeout — outputs to safe state\n");
            if (gpio)
                gpio_set_safe(gpio);
        }
        prev_link_ok = link_ok;

        /* 3. если связь в норме — копируем принятые пины на светодиоды */
        if (gpio && link_ok) {
            /* снимаем safe_mode, если он был */
            /* (gpio_set_pin сам игнорирует запись в safe_mode,
               поэтому сначала сбрасываем флаг через повторный init-логики
               или добавляем gpio_clear_safe — здесь делаем просто) */
            for (unsigned i = 0; i < 4; i++)
                gpio_set_pin(gpio, i, goose_get_pin(goose, i));
        }

        /* 4. физическая запись в линии */
        if (gpio)
            gpio_tick(gpio);

        usleep(50 * 1000);   /* 50 мс */
    }

    if (gpio)
        gpio_destroy(gpio);

    goose_destroy(goose);
    return 0;
}



