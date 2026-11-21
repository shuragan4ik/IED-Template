/*
 *  goose_subscriber.c
 *
 *  Copyright 2013-2024 Michael Zillgith
 *
 *  This file is part of libIEC61850.
 *
 *  libIEC61850 is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  libIEC61850 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with libIEC61850.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  See COPYING file for the complete license text.
 */

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include "libiec61850_platform_includes.h"
#include <string.h>

#include "goose.h"
#include "goose_receiver.h"
#include "stack_config.h"
#include "goose_subscriber.h"
#include "hal_ethernet.h"
#include "hal_thread.h"
#include "linked_list.h"

#include "ber_decode.h"

#include "mms_value.h"
#include "mms_value_internal.h"

#include "goose_receiver_internal.h"

typedef struct goose_ctx
{
    GooseReceiver receiver;
    GooseSubscriber subscriber;
    GooseListener listener;      /* издатель libiec61850 */
    LinkedList    *dataSet;        /* набор данных: 4 MmsValue (BOOLEAN) */
    bool           pins[4];        /* текущее состояние 4 значений датасета */
    uint32_t       lastStNum;       /*  прошлый номер состояния */
    uint32_t       lastSqNum;       /*последний кадр*/
    uint32_t       timeAllowedToLive;
    uint64_t       lastRxMs;            /*время последнего приема*/
    bool           isInitialized;       /*  был ли принят хотя бы один пакет */
    bool           commLost;
    uint64_t       lastPublishMs;  /* время последней отправки, мс */
    volatile bool newMessage;
} goose_ctx;

goose_ctx ctx;

static int running = 1;

static void
sigint_handler(int signalId)
{
    running = 0;
}

static void
goose_ctx->isInitialized     = true;
    goose_ctx->commLost          = false; // Связь активна

    if (dataChanged) {
        goose_ctx->newMessage = true;
    }
}

void*
Goose_create(char* goCbRef, MmsValue* dataSetValues)
{
    memset(&ctx, 0, sizeof(goose_ctx));

    ctx.receiver = GooseReceiver_create();

    GooseReceiver_setInterfaceId(ctx.receiver, ethport);

    ctx.subscriber = GooseSubscriber_create(goCbRef, NULL);

    uint8_t dstMac[6] = {0x01,0x0c,0xcd,0x01,0x00,0x01};
    GooseSubscriber_setDstMac(ctx.subscriber, dstMac);
    GooseSubscriber_setAppId(ctx.subscriber, 1000);

    GooseSubscriber_setListener(ctx.subscriber, gooseListener, &ctx);

    GooseReceiver_addSubscriber(ctx.receiver, ctx.subscriber);

    GooseReceiver_start(ctx.receiver);
    printf("ctx_created:\n");

    return &ctx;
};

bool goose_tick(void *ctx)
{
    goose_ctx* goose_ctx = (goose_ctx*)ctx;
    bool hadNewMessage = false;

    
    if (goose_ctx->newMessage) {
        goose_ctx->newMessage = false;
        hadNewMessage = true;

        uint8_t value = (goose_ctx->pins ? 1 : 0) |
                        ((goose_ctx->pins ? 1 : 0) << 1) |
                        ((goose_ctx->pins ? 1 : 0) << 2) |
                        ((goose_ctx->pins ? 1 : 0) << 3);

        printf("[RX] stNum=%u sqNum=%u pins=%d %d %d %d -> gpio=0b%d%d%d%d (%u)\n",
               goose_ctx->lastStNum,
               goose_ctx->lastSqNum,
               goose_ctx->pins, goose_ctx->pins, goose_ctx->pins, goose_ctx->pins,
               goose_ctx->pins, goose_ctx->pins, goose_ctx->pins, goose_ctx->pins,
               value);
    }

    
    if (goose_ctx->isInitialized && !goose_ctx->commLost) {
        uint64_t nowMs = Hal_getTimeInMs();
        if (nowMs > (goose_ctx->lastRxMs + goose_ctx->timeAllowedToLive)) {
            goose_ctx->commLost = true;
            printf("[ALARM] GOOSE timeout! Link lost.\n");
        }
    }

    return hadNewMessage;
}

void goose_destroy(void *ctx)
{
    goose_ctx* goose_ctx = ctx;

    GooseReceiver_stop(goose_ctx->receiver);
    
    GooseReceiver_destroy(goose_ctx->receiver);
    printf("goose_destroy:\n");
    
};

void*
Goose_create(char* goCbRef, MmsValue* dataSetValues)
{
    memset(&ctx, 0, sizeof(goose_ctx));

    ctx.receiver = GooseReceiver_create();

    GooseReceiver_setInterfaceId(ctx.receiver, ethport);

    ctx.subscriber = GooseSubscriber_create(goCbRef, NULL);

    uint8_t dstMac[6] = {0x01,0x0c,0xcd,0x01,0x00,0x01};
    GooseSubscriber_setDstMac(ctx.subscriber, dstMac);
    GooseSubscriber_setAppId(ctx.subscriber, 1000);

    GooseSubscriber_setListener(ctx.subscriber, gooseListener, &ctx);

    GooseReceiver_addSubscriber(ctx.receiver, ctx.subscriber);

    GooseReceiver_start(ctx.receiver);
    printf("ctx_created:\n");

    return &ctx;
};

bool goose_tick(void *ctx)
{
    goose_ctx* goose_ctx = (goose_ctx*)ctx;
    bool hadNewMessage = false;

  
    if (goose_ctx->newMessage) {
        goose_ctx->newMessage = false;
        hadNewMessage = true;

        uint8_t value = (goose_ctx->pins ? 1 : 0) |
                        ((goose_ctx->pins ? 1 : 0) << 1) |
                        ((goose_ctx->pins ? 1 : 0) << 2) |
                        ((goose_ctx->pins ? 1 : 0) << 3);

        printf("[RX] stNum=%u sqNum=%u pins=%d %d %d %d -> gpio=0b%d%d%d%d (%u)\n",
               goose_ctx->lastStNum,
               goose_ctx->lastSqNum,
               goose_ctx->pins, goose_ctx->pins, goose_ctx->pins, goose_ctx->pins,
               goose_ctx->pins, goose_ctx->pins, goose_ctx->pins, goose_ctx->pins,
               value);
    }

   
    if (goose_ctx->isInitialized && !goose_ctx->commLost) {
        uint64_t nowMs = Hal_getTimeInMs();
        if (nowMs > (goose_ctx->lastRxMs + goose_ctx->timeAllowedToLive)) {
            goose_ctx->commLost = true;
            printf("[ALARM] GOOSE timeout! Link lost.\n");
        }
    }

    return hadNewMessage;
}

void goose_destroy(void *ctx)
{
    goose_ctx* goose_ctx = ctx;

    GooseReceiver_stop(goose_ctx->receiver);
    
    GooseReceiver_destroy(goose_ctx->receiver);
    printf("goose_destroy:\n");
    
};