#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <gpiod.h>

#include "gpio.h"

/* ============================================================
 * Настройки под конкретную плату — ИЗМЕНИТЕ ПОД СВОЁ ЖЕЛЕЗО
 * ============================================================ */
#define GPIO_CHIP_PATH      "/dev/gpiochip0"

/* Offset линий внутри чипа */
static const unsigned GPIO_LINES[4] = {
    17,     /* pin 0 */
    27,     /* pin 1 */
    22,     /* pin 2 */
    23      /* pin 3 */
};

#define CONSUMER_NAME       "ied-subscriber"

/* ============================================================ */

struct gpio_ctx {
    struct gpiod_chip          *chip;
    struct gpiod_line_request  *request;   /* один request на 4 линии */

    bool    pins[4];        /* желаемое состояние */
    bool    applied[4];     /* что реально записано */
    bool    safe_mode;
    bool    initialized;
};

gpio_ctx *
gpio_init(void)
{
    gpio_ctx *ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
        return NULL;

    /* --- открываем чип --- */
    ctx->chip = gpiod_chip_open(GPIO_CHIP_PATH);
    if (!ctx->chip) {
        fprintf(stderr, "[GPIO] cannot open %s\n", GPIO_CHIP_PATH);
        free(ctx);
        return NULL;
    }

    /* --- настройки линий --- */
    struct gpiod_line_settings *settings = gpiod_line_settings_new();
    if (!settings) {
        fprintf(stderr, "[GPIO] gpiod_line_settings_new failed\n");
        goto fail;
    }

    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE); /* безопасное = 0 */

    struct gpiod_line_config *line_cfg = gpiod_line_config_new();
    if (!line_cfg) {
        fprintf(stderr, "[GPIO] gpiod_line_config_new failed\n");
        gpiod_line_settings_free(settings);
        goto fail;
    }

    /* добавляем все 4 линии с одинаковыми настройками */
    for (int i = 0; i < 4; i++) {
        if (gpiod_line_config_add_line_settings(line_cfg,
                                                &GPIO_LINES[i], 1,
                                                settings) < 0) {
            fprintf(stderr, "[GPIO] cannot add line %u\n", GPIO_LINES[i]);
            gpiod_line_config_free(line_cfg);
            gpiod_line_settings_free(settings);
            goto fail;
        }
    }

    gpiod_line_settings_free(settings);

    /* --- request config --- */
    struct gpiod_request_config *req_cfg = gpiod_request_config_new();
    if (!req_cfg) {
        fprintf(stderr, "[GPIO] gpiod_request_config_new failed\n");
        gpiod_line_config_free(line_cfg);
        goto fail;
    }
    gpiod_request_config_set_consumer(req_cfg, CONSUMER_NAME);

    /* --- запрашиваем линии --- */
    ctx->request = gpiod_chip_request_lines(ctx->chip, req_cfg, line_cfg);

    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);

    if (!ctx->request) {
        fprintf(stderr, "[GPIO] gpiod_chip_request_lines failed\n");
        goto fail;
    }

    for (int i = 0; i < 4; i++) {
        ctx->pins[i]    = false;
        ctx->applied[i] = false;
    }

    ctx->safe_mode   = false;
    ctx->initialized = true;

    printf("[GPIO] initialized (v2), chip=%s, lines=%u %u %u %u (safe state)\n",
           GPIO_CHIP_PATH,
           GPIO_LINES[0], GPIO_LINES[1], GPIO_LINES[2], GPIO_LINES[3]);

    return ctx;

fail:
    if (ctx->chip)
        gpiod_chip_close(ctx->chip);
    free(ctx);
    return NULL;
}

void
gpio_set_pin(gpio_ctx *ctx, unsigned index, bool value)
{
    if (!ctx || !ctx->initialized || index >= 4)
        return;

    if (ctx->safe_mode)
        return;

    ctx->pins[index] = value;
}

void
gpio_set_safe(gpio_ctx *ctx)
{
    if (!ctx || !ctx->initialized)
        return;

    ctx->safe_mode = true;

    for (int i = 0; i < 4; i++)
        ctx->pins[i] = false;

    /* сразу гасим все линии */
    enum gpiod_line_value values[4] = {
        GPIOD_LINE_VALUE_INACTIVE,
        GPIOD_LINE_VALUE_INACTIVE,
        GPIOD_LINE_VALUE_INACTIVE,
        GPIOD_LINE_VALUE_INACTIVE
    };

    if (gpiod_line_request_set_values(ctx->request, values) == 0) {
        for (int i = 0; i < 4; i++)
            ctx->applied[i] = false;
    }

    printf("[GPIO] safe state applied (all outputs OFF)\n");
}

void
gpio_clear_safe(gpio_ctx *ctx)
{
    if (!ctx || !ctx->initialized)
        return;

    ctx->safe_mode = false;
    printf("[GPIO] safe mode cleared\n");
}

void
gpio_tick(gpio_ctx *ctx)
{
    if (!ctx || !ctx->initialized)
        return;

    /* проверяем, нужно ли что-то менять */
    bool need_update = false;
    for (int i = 0; i < 4; i++) {
        if (ctx->pins[i] != ctx->applied[i]) {
            need_update = true;
            break;
        }
    }
    if (!need_update)
        return;

    enum gpiod_line_value values[4];
    for (int i = 0; i < 4; i++)
        values[i] = ctx->pins[i] ? GPIOD_LINE_VALUE_ACTIVE
                                  : GPIOD_LINE_VALUE_INACTIVE;

    if (gpiod_line_request_set_values(ctx->request, values) == 0) {
        for (int i = 0; i < 4; i++)
            ctx->applied[i] = ctx->pins[i];
    } else {
        fprintf(stderr, "[GPIO] failed to set values\n");
    }
}

void
gpio_destroy(gpio_ctx *ctx)
{
    if (!ctx)
        return;

    if (ctx->initialized) {
        /* перед освобождением — безопасное состояние */
        enum gpiod_line_value values[4] = {
            GPIOD_LINE_VALUE_INACTIVE,
            GPIOD_LINE_VALUE_INACTIVE,
            GPIOD_LINE_VALUE_INACTIVE,
            GPIOD_LINE_VALUE_INACTIVE
        };
        gpiod_line_request_set_values(ctx->request, values);

        gpiod_line_request_release(ctx->request);
        gpiod_chip_close(ctx->chip);

        printf("[GPIO] destroyed, outputs set to safe state\n");
    }

    free(ctx);
}