#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "libiec61850_platform_includes.h"

#include "goose.h"
#include "goose_receiver.h"
#include "stack_config.h"
#include "goose_subscriber.h"
#include "hal_ethernet.h"
#include "hal_thread.h"
#include "hal_time.h"
#include "linked_list.h"

#include "ber_decode.h"

#include "mms_value.h"
#include "mms_value_internal.h"

#include "goose_receiver_internal.h"

typedef struct goose_ctx
{
    GooseReceiver   receiver;
    GooseSubscriber subscriber;
    bool            pins[4];
    uint32_t        lastStNum;
    uint32_t        lastSqNum;
    uint32_t        timeAllowedToLive;   /* мс, из последнего кадра */
    uint64_t        lastRxMs;            /* время последнего приёма */
    bool            hasLastFrame;        /* уже принимали хотя бы один кадр */
    bool            linkFailed;          /* авария связи */
    volatile bool   newMessage;          /* флаг для goose_tick */
} goose_ctx;

goose_ctx ctx;

/* ------------------------------------------------------------------ */
/*  Обработчик, вызываемый библиотекой при каждом принятом GOOSE      */
/* ------------------------------------------------------------------ */
static void
gooseListener(GooseSubscriber subscriber, void *parameter)
{
    goose_ctx *g = (goose_ctx *)parameter;

    uint32_t stNum = GooseSubscriber_getStNum(subscriber);
    uint32_t sqNum = GooseSubscriber_getSqNum(subscriber);
    uint32_t tal   = GooseSubscriber_getTimeAllowedToLive(subscriber);
    uint64_t now   = Hal_getTimeInMs();

    /* --- разбор датасета (строго 4 BOOLEAN) --- */
    MmsValue *values = GooseSubscriber_getDataSetValues(subscriber);
    if (!values) {
        printf("[RX] no dataset\n");
        return;
    }

    int arraySize = MmsValue_getArraySize(values);
    if (arraySize != 4) {
        printf("[RX] unexpected dataset size: %d (expected 4)\n", arraySize);
        return;
    }

    bool newPins[4];
    for (int i = 0; i < 4; i++) {
        MmsValue *el = MmsValue_getElement(values, i);
        if (MmsValue_getType(el) != MMS_BOOLEAN) {
            printf("[RX] element %d is not BOOLEAN\n", i);
            return;
        }
        newPins[i] = MmsValue_getBoolean(el);
    }

    /* --- классификация сообщения --- */
    bool stateChanged = false;
    bool isRetransmission = false;
    int  lostCount = 0;

    if (!g->hasLastFrame) {
        /* первое сообщение */
        stateChanged = true;
    }
    else if (stNum != g->lastStNum) {
        /* stNum изменился → новое состояние */
        if (stNum < g->lastStNum) {
            /* stNum уменьшился — издатель, скорее всего, перезапустился */
            printf("[RX] stNum decreased (%u → %u) — publisher restart?\n",
                   g->lastStNum, stNum);
        }
        stateChanged = true;
    }
    else {
        /* stNum тот же */
        if (sqNum == g->lastSqNum) {
            /* полный дубликат — игнорируем */
            return;
        }
        else if (sqNum == g->lastSqNum + 1) {
            /* нормальная ретрансляция / heartbeat */
            isRetransmission = true;

            /* датасет должен совпадать */
            bool mismatch = false;
            for (int i = 0; i < 4; i++) {
                if (newPins[i] != g->pins[i]) {
                    mismatch = true;
                    break;
                }
            }
            if (mismatch) {
                printf("[RX] WARNING: retransmission with different dataset "
                       "(stNum=%u sqNum=%u)\n", stNum, sqNum);
            }
        }
        else if (sqNum > g->lastSqNum + 1) {
            /* пропущены кадры */
            lostCount = (int)(sqNum - g->lastSqNum - 1);
            printf("[RX] LOST %d frame(s) (sqNum %u → %u)\n",
                   lostCount, g->lastSqNum, sqNum);
            /* считаем это обновлением состояния на всякий случай */
            stateChanged = true;
        }
        else {
            /* sqNum уменьшился при том же stNum — аномалия */
            printf("[RX] anomalous sqNum (%u → %u) at stNum=%u\n",
                   g->lastSqNum, sqNum, stNum);
            stateChanged = true;
        }
    }

    /* --- сохраняем новое состояние --- */
    for (int i = 0; i < 4; i++)
        g->pins[i] = newPins[i];

    g->lastStNum         = stNum;
    g->lastSqNum         = sqNum;
    g->timeAllowedToLive = tal;
    g->lastRxMs          = now;
    g->hasLastFrame      = true;
    g->linkFailed        = false;          /* связь восстановилась */
    g->newMessage        = true;           /* сообщить tick */

    /* --- краткий лог прямо в listener (детальный — в tick) --- */
    if (stateChanged) {
        printf("[RX] STATE CHANGE  stNum=%u sqNum=%u\n", stNum, sqNum);
    } else if (isRetransmission) {
        printf("[RX] RETRANSMISSION stNum=%u sqNum=%u\n", stNum, sqNum);
    }
}

/* ------------------------------------------------------------------ */
void *
Goose_create(char *goCbRef, MmsValue *dataSetValues)
{
    (void)goCbRef;
    (void)dataSetValues;

    memset(&ctx, 0, sizeof(ctx));

    ctx.receiver = GooseReceiver_create();
    if (!ctx.receiver) {
        printf("GooseReceiver_create failed\n");
        return NULL;
    }

    GooseReceiver_setInterfaceId(ctx.receiver, ethport);

    ctx.subscriber = GooseSubscriber_create(
            "simpleIOGenericIO/LLN0$GO$gcbAnalogValues", NULL);

    uint8_t dstMac[6] = {0x01, 0x0c, 0xcd, 0x01, 0x00, 0x01};
    GooseSubscriber_setDstMac(ctx.subscriber, dstMac);
    GooseSubscriber_setAppId(ctx.subscriber, 1000);

    GooseSubscriber_setListener(ctx.subscriber, gooseListener, &ctx);
    GooseReceiver_addSubscriber(ctx.receiver, ctx.subscriber);

    GooseReceiver_start(ctx.receiver);

    if (!GooseReceiver_isRunning(ctx.receiver)) {
        printf("Failed to start GOOSE subscriber "
               "(check interface / root privileges)\n");
        GooseReceiver_destroy(ctx.receiver);
        return NULL;
    }

    printf("GOOSE subscriber started on %s\n", ethport);
    return &ctx;
}

/* ------------------------------------------------------------------ */
bool
goose_get_pin(const void *ctx_ptr, unsigned index)
{
    if (!ctx_ptr || index >= 4)
        return false;

    const goose_ctx *g = (const goose_ctx *)ctx_ptr;
    return g->pins[index];
}

/* ------------------------------------------------------------------ */
bool
goose_tick(void *ctx_ptr)
{
    if (!ctx_ptr)
        return false;

    goose_ctx *g = (goose_ctx *)ctx_ptr;
    uint64_t now = Hal_getTimeInMs();

    /* --- 1. обработка нового сообщения --- */
    bool was_new = g->newMessage;
    if (was_new) {
        g->newMessage = false;

        /* собираем 4-битное значение */
        unsigned gpio = 0;
        for (int i = 0; i < 4; i++)
            if (g->pins[i])
                gpio |= (1u << i);

        printf("[RX] stNum=%u sqNum=%u  pins=%d %d %d %d  -> gpio=0b%04b (%u)\n",
               g->lastStNum, g->lastSqNum,
               g->pins[0], g->pins[1], g->pins[2], g->pins[3],
               gpio, gpio);
    }

    /* --- 2. контроль обрыва связи --- */
    if (g->hasLastFrame && !g->linkFailed) {
        if (now > g->lastRxMs + g->timeAllowedToLive) {
            g->linkFailed = true;
            printf("[RX] LINK FAILURE — no GOOSE for > %u ms "
                   "(lastRx=%llu, now=%llu)\n",
                   g->timeAllowedToLive,
                   (unsigned long long)g->lastRxMs,
                   (unsigned long long)now);
        }
    }

    return was_new;
}

/* ------------------------------------------------------------------ */
void
goose_destroy(void *ctx_ptr)
{
    if (!ctx_ptr)
        return;

    goose_ctx *g = (goose_ctx *)ctx_ptr;

    GooseReceiver_stop(g->receiver);
    GooseReceiver_destroy(g->receiver);
    printf("goose destroyed\n");
}

bool
goose_link_ok(const void *ctx_ptr)
{
    if (!ctx_ptr)
        return false;

    const goose_ctx *g = (const goose_ctx *)ctx_ptr;

    /* пока не было ни одного кадра — считаем связь ещё не установленной */
    if (!g->hasLastFrame)
        return false;

    return !g->linkFailed;
}