#include "from_gui.h"

#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <cstdlib>
#include <iostream>

extern int sock_tcp;
extern uint8_t direction;
extern sem_t direction_mutex;

void *from_gui(void*) {

    char buffer[32];
    int rcv_len;

    for (;;) {
        memset(buffer, 0, sizeof(buffer));
        rcv_len = read(sock_tcp, buffer, sizeof(buffer) - 1);
        if (rcv_len < 0) {
            exit(EXIT_FAILURE);
        }

        sem_wait(&direction_mutex);

        if (rcv_len == 14 && !strncmp(buffer, "LEFT_KEY_DOWN\n", rcv_len)) {
            direction = 2;
        } else if (rcv_len == 12 && !strncmp(buffer, "LEFT_KEY_UP\n", rcv_len)) {
            if (direction == 2) {
                direction = 0;
            }
        } else if (rcv_len == 15 && !strncmp(buffer, "RIGHT_KEY_DOWN\n", rcv_len)) {
            direction = 1;
        } else if (rcv_len == 13 && !strncmp(buffer, "RIGHT_KEY_UP\n", rcv_len)) {
            if (direction == 1) {
                direction = 0;
            }
        }

        sem_post(&direction_mutex);
    }
}