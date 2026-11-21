/*
 * goose_publisher_example.c
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <signal.h>
#include  <unistd.h>

#include "goose.h"

static volatile sig_atomic_t running = 1;

static void sigint_handler(int sig)
{
    (void)sig;
    running = 0;
    printf("running = 0:\n");
}

int
main(int argc, char** argv)
{
    void* goose_ctx = Goose_create();
    
    signal(SIGINT, sigint_handler);

    while(running == 1)
    {
        bool newValue = goose_tick(goose_ctx);

        usleep(1000000);
    }

    goose_destroy(goose_ctx);
    return 0;
}