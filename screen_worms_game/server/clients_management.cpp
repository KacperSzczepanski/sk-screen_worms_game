#include <netinet/in.h>
#include <stdio.h>
#include <string>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <fstream>
#include <thread>
#include <chrono>
#include <netdb.h>
#include <stdlib.h>
#include <variant>
#include <netinet/tcp.h>
#include <set>
#include <semaphore.h>
#include <pthread.h>
#include <string.h>
#include <map>

#include "clients_management.h"
#include "structs.h"

extern bool operator<(player_data_t player1, player_data_t player2);
extern bool operator<(sockaddr_in6 addr1, sockaddr_in6 addr2);
extern bool operator==(sockaddr_in6 addr1, sockaddr_in6 addr2);

extern uint8_t no_of_players;
extern int sock;

extern std::map <sockaddr_in6, client_data_t> clients;
extern sem_t clients_mutex;
extern void send_datagram(sockaddr_in6 address, uint32_t from_event);
extern uint64_t get_time_now();

char buffer[MAX_MSG_SIZE + 1];
int ptr;

uint64_t read_bytes(int bytes) {
    uint64_t result = 0;

    for (int i = 0; i < bytes; ++i) {
        result <<= 8;
        result += (uint8_t)buffer[ptr++];
    }

    return result;
}

bool check_if_player_is_connected_somewhere_else(sockaddr_in6 address, const std::string &player_name) {
    for (auto it = clients.begin(); it != clients.end(); ++it) {
        if (it->first == address) {
            continue;
        }

        if (it->second.player_name == player_name) {
            return true;
        }
    }

    return false;
}

void *clients_management(void*) {

    for (;;) {
        ptr = 0;
        memset (buffer, 0, MAX_MSG_SIZE + 1);

        struct sockaddr_in6 client_address;
        socklen_t addr_len = sizeof(struct sockaddr_in6);
        
        int rcv_len = recvfrom(sock, buffer, MAX_MSG_SIZE + 1, 0, (struct sockaddr *)&client_address, &addr_len);

        if (rcv_len < MIN_MSG_SIZE || MAX_MSG_SIZE < rcv_len) {
            continue;
        }

        uint64_t session_id = read_bytes(8);
        uint8_t direction = read_bytes(1);
        uint32_t next_expected_event_no = read_bytes(4);
        uint64_t now = get_time_now();

        std::string player_name = "";
        bool correct_name = true;
        for (int i = MIN_MSG_SIZE; i < rcv_len; ++i) {
            if ((int)buffer[ptr] < MIN_ASCII_ALLOWED || MAX_ASCII_ALLOWED < (int)buffer[ptr]) {
                correct_name = false;
                break;
            }

            player_name += buffer[ptr++];
        }
        if (!correct_name) {
            continue;
        }

        sem_wait(&clients_mutex);

        if (check_if_player_is_connected_somewhere_else(client_address, player_name)) { //we can ignore client if he is already connected in other socket
            sem_post(&clients_mutex);
            continue;
        }

        if (clients.find(client_address) != clients.end()) { //somebody is connected here
            if (session_id < clients[client_address].session_id) { //ignore lower id
                sem_post(&clients_mutex);
                continue;
            } else if (session_id == clients[client_address].session_id) { //everything is ok
                clients[client_address].direction = direction;
                clients[client_address].next_expected_event_no = next_expected_event_no;
                clients[client_address].time_of_last_message = now;
                clients[client_address].is_ready |= (direction == 1 || direction == 2);
            } else { //disconnect and connect new client
                if (!player_name.empty() && clients[client_address].player_name.empty()) {
                    if (no_of_players >= 25) {
                        sem_post(&clients_mutex);
                        continue;
                    } else {
                        ++no_of_players;
                    }
                } else if (player_name.empty() && !clients[client_address].player_name.empty()) {
                    --no_of_players;
                }
                clients[client_address].session_id = session_id;
                clients[client_address].direction = direction;
                clients[client_address].next_expected_event_no = next_expected_event_no;
                clients[client_address].player_name = player_name;
                clients[client_address].time_of_last_message = now;
                clients[client_address].is_ready = (direction == 1 || direction == 2);
            }
        } else { //nobody is connected here
            if (!player_name.empty()) {
                if (no_of_players >= 25) {
                    sem_post(&clients_mutex);
                    continue;
                } else {
                    ++no_of_players;
                }
            }

            clients[client_address] = client_data_t {
                session_id : session_id,
                direction : direction,
                next_expected_event_no : next_expected_event_no,
                player_name : player_name,
                time_of_last_message : now,
                is_ready : (direction == 1 || direction == 2),
            };
        }
        sem_post(&clients_mutex);

        send_datagram(client_address, next_expected_event_no);
        
    }
}