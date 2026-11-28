#ifndef GPIO_H
#define GPIO_H

#include <stdbool.h>

struct gpio_ctx;
typedef struct gpio_ctx gpio_ctx;

/**
 * Запросить 4 линии как выходы.
 * Начальное состояние — безопасное (все выключены).
 * При ошибке возвращает NULL.
 */
gpio_ctx *gpio_init(void);

/**
 * Обновить теневое значение пина (0..3).
 * Физическая запись произойдёт в gpio_tick().
 */
void gpio_set_pin(gpio_ctx *ctx, unsigned index, bool value);

/**
 * Перевести все выходы в безопасное состояние (погасить светодиоды)
 * и заблокировать дальнейшие gpio_set_pin до снятия аварии.
 */
void gpio_set_safe(gpio_ctx *ctx);

/**
 * Записать отложенные значения в линии (неблокирующе).
 */
void gpio_tick(gpio_ctx *ctx);

/**
 * Перед освобождением переводит выходы в безопасное состояние,
 * затем освобождает линии и чип.
 */
void gpio_destroy(gpio_ctx *ctx);

void gpio_clear_safe(gpio_ctx *ctx);   /* снять аварийный режим */

#endif /* GPIO_H */