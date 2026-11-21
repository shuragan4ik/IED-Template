#define ethport "eth0"

bool  goose_get_pin(const void *ctx, unsigned index); /* прочитать значение датасета (0..3) */
bool goose_tick(void *ctx);               /* проверить, не пришли ли новые GOOSE-сообщения */
void  goose_destroy(void *ctx);            /* остановить и освободить ресурсы */
void* Goose_create();
