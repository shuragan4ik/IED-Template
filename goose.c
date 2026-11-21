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
gooseListener(GooseSubscriber subscriber, void* parameter)
{
    //GPIO* gpio=static_cast<GPIO*>(parameter);
    if (!GooseSubscriber_isValid(subscriber)) {
        return;
    }
    uint32_t currentStNum = GooseSubscriber_getStNum(subscriber);
    goose_ctx* goose_ctx = parameter;

    printf("GOOSE event:\n");
    printf("  stNum: %u sqNum: %u\n", 
            GooseSubscriber_getStNum(subscriber),
            GooseSubscriber_getSqNum(subscriber));
    printf("  timeToLive: %u\n", GooseSubscriber_getTimeAllowedToLive(subscriber));

    uint64_t timestamp = GooseSubscriber_getTimestamp(subscriber);

    printf("  timestamp: %u.%u\n", 
            (uint32_t) (timestamp / 1000), (uint32_t) (timestamp % 1000));
    printf("  message is %s\n", GooseSubscriber_isValid(subscriber) ? "valid" : "INVALID");

    MmsValue* values = GooseSubscriber_getDataSetValues(subscriber);

    char buffer[1024];

    MmsValue_printToBuffer(values, buffer, sizeof(buffer));

    printf("  allData: %s\n", buffer);

       
    int i;

    bool dataChanged = false;
    int arraySize = MmsValue_getArraySize(values);
    if (arraySize == 4) { // от выхода за пределы 4 пинов

    for (int i = 0; i < arraySize; i++) {
        MmsValue *pin_value = MmsValue_getElement(values, i);
        bool pin_state = MmsValue_getBoolean(pin_value);

        
        if (!goose_ctx->isInitialized || goose_ctx->pins[i] != pin_state) {     // Если значение пина изменилось
            dataChanged = true;
        }
        goose_ctx->pins[i] = pin_state;
    } } else {
        printf(" unexpected dataset size: %d\n", arraySize);
    }

    
    if (!goose_ctx->isInitialized || currentStNum != goose_ctx->lastStNum) {     //  Изменился ли stNum
        dataChanged = true;
    }

    
    if (dataChanged) {                                  //проверка на обновление данных
        goose_ctx->lastStNum = currentStNum;
        goose_ctx->isInitialized = true;
        goose_ctx->newMessage = true;
        printf("newMessage!\n");
    }
        // gpio->SetValue(pin_state,i);
    
}

void*
Goose_create(char* goCbRef, MmsValue* dataSetValues)
{
    ctx.receiver = GooseReceiver_create();

    GooseReceiver_setInterfaceId(ctx.receiver, ethport);

    ctx.subscriber = GooseSubscriber_create("simpleIOGenericIO/LLN0$GO$gcbAnalogValues", NULL);

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
    goose_ctx* goose_ctx = ctx;
    if (goose_ctx->newMessage);
        printf("goose_tick:\n");
        printf("pins: [%d, %d, %d, %d]\n", 
           goose_ctx->pins[0], 
           goose_ctx->pins[1], 
           goose_ctx->pins[2], 
           goose_ctx->pins[3]);
        {
        goose_ctx->newMessage = false;                  // Сбрасываем флаг
        return true;
    }
    return false;
    
}

void goose_destroy(void *ctx)
{
    goose_ctx* goose_ctx = ctx;

    GooseReceiver_stop(goose_ctx->receiver);
    
    GooseReceiver_destroy(goose_ctx->receiver);
    printf("goose_destroy:\n");
    
};