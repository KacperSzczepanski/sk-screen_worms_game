#include "to_server.h"

#include <string.h>
#include <unistd.h>
#include <vector>
#include <chrono>
#include <semaphore.h>
#include <iostream>

using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::system_clock;

pthread_t send_datagram_thread;
pthread_attr_t send_attr;

extern uint64_t session_id;
extern int sock_udp;
extern uint8_t direction;
extern uint32_t next_expected_event_no;
extern sem_t direction_mutex, next_expected_event_mutex;
extern std::string player_name;

void *to_server(void*) {

    uint64_t sent_datagrams = 0;
    uint64_t last_sent = session_id;

    for (;;) {
        auto now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        uint64_t now_u = static_cast<uint64_t>(now);

        if (now_u - last_sent >= MESSAGE_INTERVAL) {
            last_sent = now_u;
            ++sent_datagrams;

            std::vector <uint8_t> datagram;
            int snd_len;

            for (int i = 7; i >= 0; --i) {
                datagram.push_back((uint8_t)(session_id >> (i * 8)));
            }

            sem_wait(&direction_mutex);
            uint8_t direction_now = direction;
            sem_post(&direction_mutex);
            datagram.push_back(direction_now);

            sem_wait(&next_expected_event_mutex);
            uint32_t next_event_now = next_expected_event_no;
            sem_post(&next_expected_event_mutex);

            for (int i = 3; i >= 0; --i) {
                datagram.push_back((uint8_t)(next_event_now >> (i * 8)));
            }

            for (const char &c : player_name) {
                datagram.push_back((uint8_t)c);
            }

            snd_len = write(sock_udp, datagram.data(), datagram.size());
            
            if (snd_len != (int)datagram.size()) {
                std::cerr << "Failed to send datagram to server.\n";
                exit(EXIT_FAILURE);
            }
        } else {
            usleep((MESSAGE_INTERVAL - (now_u - last_sent)) * 1000);
        }
    }
}