#ifndef GOOSE_H
#define GOOSE_H

#include <stdbool.h>
#include "mms_value.h"

#define ethport "eth0"

void* Goose_create(char* goCbRef, MmsValue* dataSetValues);
bool  goose_get_pin(const void *ctx, unsigned index); /* прочитать значение датасета (0..3) */
bool  goose_tick(void *ctx);               /* проверить, не пришли ли новые GOOSE-сообщения */
void  goose_destroy(void *ctx);            /* остановить и освободить ресурсы */
bool goose_link_ok(const void *ctx);   /* true = связь в норме */
#endif