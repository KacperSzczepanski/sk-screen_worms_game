#include <sys/types.h>
#include <sys/socket.h>
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
#include <vector>
#include <map>
#include <algorithm>
#include <math.h>

#include "game_management.h"
#include "structs.h"
#include "events.h"
#include "crc32_table.h"

extern bool operator<(player_data_t player1, player_data_t player2);
extern bool operator<(sockaddr_in6 addr1, sockaddr_in6 addr2);
extern bool operator==(sockaddr_in6 addr1, sockaddr_in6 addr2);

extern uint32_t port, seed, turning_speed, rounds_per_sec, width, height;
extern sem_t clients_mutex;
extern sem_t events_mutex;
extern std::map <sockaddr_in6, client_data_t> clients;
extern uint8_t no_of_players;
extern uint64_t get_time_now();

extern int sock;

std::vector <std::vector <uint8_t>> events;
std::vector <player_data_t> players;

uint32_t game_id, no_of_events;
uint64_t now;
bool game_started, everyone_is_ready;
std::vector <std::vector <bool>> board;
uint64_t last_sent, start_of_game;
uint8_t no_of_players_alive;
uint32_t rand_result;
bool rand_runned = false;

uint32_t game_rand() {
    if (!rand_runned) {
        rand_runned = true;
        rand_result = seed;
    } else {
        uint64_t result = rand_result;

        result = (result * 279410273) % 4294967291;

        rand_result = (uint32_t)result;
    }

    return rand_result;
}

uint32_t crc32(const uint8_t *buf, size_t size)
{
    const uint8_t *p = buf;
    uint32_t crc;

    crc = ~0U;

    while (size--)
        crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);

    return crc ^ ~0U;
}

void send_datagram(sockaddr_in6 address, uint32_t from_event) {
    int64_t max_event = (int64_t)no_of_events - 1;
    if (max_event < from_event) {
        return;
    }
    
    sem_wait(&events_mutex);
    max_event = (int64_t)no_of_events - 1;

    if (from_event > max_event) {
        sem_post(&events_mutex);
        return;
    }

    std::vector <uint8_t> datagram;

    for (int i = 3; i >= 0; --i) {
        datagram.push_back((uint8_t)(game_id >> (i * 8)));
    }

    for (int64_t i = from_event; i <= max_event; ++i) {
        
        if (datagram.size() + events[i].size() + 12 > MAX_DATAGRAM_SIZE) {
            break;
        } 
        std::vector <uint8_t> event;

        uint32_t len = (uint32_t)events[i].size() + 4;

        for (int j = 3; j >= 0; --j) {
            event.push_back((uint8_t)(len >> (j * 8)));
        }

        for (int j = 3; j >= 0; --j) {
            event.push_back((uint8_t)(i >> (j * 8)));
        }

        for (int j = 0; j < (int)events[i].size(); ++j) {
            event.push_back(events[i][j]);
        }

        uint32_t crc = crc32(event.data(), event.size());

        for (int j = 3; j >= 0; --j) {
            event.push_back((uint8_t)(crc >> (j * 8)));
        }

        for (int j = 0; j < (int)event.size(); ++j) {
            datagram.push_back((uint8_t)event[j]);
        }
    }

    int len = sendto(sock, datagram.data(), datagram.size(), 0, (const struct sockaddr *)&address, (socklen_t)sizeof(address));
    ++len;

    sem_post(&events_mutex);
}

void send_to_everyone(uint32_t from_event) {
    sem_wait(&clients_mutex);

    std::vector <sockaddr_in6> receivers;

    for (auto it = clients.begin(); it != clients.end(); ++it) {
        receivers.push_back(it->first);
    }

    sem_post(&clients_mutex);

    for (int i = 0; i < (int)receivers.size(); ++i) {
        send_datagram(receivers[i], from_event);
    }
}

void new_event(std::vector <uint8_t> event) {
    sem_wait(&events_mutex);
    events.push_back(event);
    ++no_of_events;
    uint32_t event_to_send = events.size();
    sem_post(&events_mutex);
    send_to_everyone(event_to_send - 1);
}

void choose_players() {
    players.clear();

    for (auto it = clients.begin(); it != clients.end(); ++it) {
        if (it->second.player_name.empty()) {
            continue;
        }

        if (!it->second.is_ready) { //every player should be ready
            everyone_is_ready = false;
            break;
        }

        player_data_t new_player = {
            name : it->second.player_name,
            x : 0,
            y : 0,
            is_eliminated : false,
            direction : 0,
            addr : it->first,
        };

        players.push_back(new_player);
    }

    std::sort(players.begin(), players.end());
}

std::vector <std::string> get_players_names_list() {
    std::vector <std::string> players_names_list;

    for (player_data_t player : players) {
        players_names_list.push_back(player.name);
    }

    return players_names_list;
}

bool check_if_pixel_is_eaten(double x, double y) {
    return board[uint32_t(x)][uint32_t(y)];
}

bool check_if_is_outside_of_board(double x, double y) {
    if (x < 0 || y < 0 || width <= uint32_t(x) || height <= uint32_t(y)) {
        return true;
    }

    return false;
}

void set_board_cell(double x, double y, bool val) {
    board[uint32_t(x)][uint32_t(y)] = val;
}

void game_over() {
    game_started = false;
    
    new_event(game_over_event());

    sem_wait(&clients_mutex);
    for (auto it = clients.begin(); it != clients.end(); ++it) {
        it->second.is_ready = false;
    }
    sem_post(&clients_mutex);
}

void create_new_game() {
    game_id = game_rand();

   new_event(new_game_event(width, height, get_players_names_list()));

    for (uint32_t i = 0; i < width; ++i) {
        for (uint32_t j = 0; j < height; ++j) {
            board[i][j] = false;
        }
    }

    for (int i = 0; i < (int)players.size(); ++i) {
        players[i].x = (double)(game_rand() % width) + 0.5;
        players[i].y = (double)(game_rand() % height) + 0.5;
        players[i].direction = game_rand() % 360;

        if (check_if_pixel_is_eaten(players[i].x, players[i].y)) {
            players[i].is_eliminated = true;
            new_event(player_eliminated_event(i));
            --no_of_players_alive;
        } else {
            players[i].is_eliminated = false;
            new_event(pixel_event(i, uint32_t(players[i].x), uint32_t(players[i].y)));
            set_board_cell(players[i].x, players[i].y, true);
        }

        if (no_of_players_alive <= 1) {
            game_over();
            break;
        }
    }
}

void move_player(uint8_t player_number) {
    players[player_number].x += std::cos((double)players[player_number].direction * M_PI / 180);
    players[player_number].y += std::sin((double)players[player_number].direction * M_PI / 180);
}

void disconnect_inactive_players() {
    sem_wait(&clients_mutex);

    std::vector <sockaddr_in6> to_disconnect;
    now = get_time_now();
    for (auto it = clients.begin(); it != clients.end(); ++it) {
        if (now - it->second.time_of_last_message > MAX_INACTIVE_TIME_MS) {
            sockaddr_in6 addr = it->first;
            to_disconnect.push_back(addr);
        }
    }

    for (int j = 0; j < (int)to_disconnect.size(); ++j) {
        if (!clients[to_disconnect[j]].player_name.empty()) {
            --no_of_players;
        }

        clients.erase(to_disconnect[j]);
    }

    sem_post(&clients_mutex);
}

void *game_management(void*) {

    for (uint32_t i = 0; i < width; ++i) {
        std::vector <bool> column;

        for (uint32_t j = 0; j < height; ++j) {
            column.push_back(false);
        }

        board.push_back(column);
    }

    for (;;) {
        disconnect_inactive_players();

        if (game_started) {
            now = get_time_now();
            if (now - last_sent > (double)1000 / rounds_per_sec) {
                last_sent = now;

                for (int i = 0; i < (int)players.size(); ++i) {
                    if (players[i].is_eliminated == true) {
                        continue;
                    }

                    uint8_t direction_clicked = 0;

                    sem_wait(&clients_mutex);
                    direction_clicked = clients[players[i].addr].direction;
                    sem_post(&clients_mutex);

                    if (direction_clicked == 1) {
                        players[i].direction = (players[i].direction + turning_speed) % 360;
                    } else if (direction_clicked == 2) {
                        players[i].direction = (players[i].direction - turning_speed + 360) % 360;
                    }

                    uint32_t cur_x = (uint32_t)players[i].x;
                    uint32_t cur_y = (uint32_t)players[i].y;
                    move_player(i);
                     if (cur_x == (uint32_t)players[i].x && cur_y == (uint32_t)players[i].y) {
                        continue;
                    }

                    if (check_if_is_outside_of_board(players[i].x, players[i].y) || check_if_pixel_is_eaten(players[i].x, players[i].y)) {
                        new_event(player_eliminated_event(i));
                        --no_of_players_alive;
                        players[i].is_eliminated = true;
                    } else {
                        new_event(pixel_event(i, uint32_t(players[i].x), uint32_t(players[i].y)));
                        set_board_cell(players[i].x, players[i].y, true);
                    }

                    if (no_of_players_alive <= 1) {
                        game_over();
                        break;
                    }
                }
            } else {
                usleep((double)1000 * (last_sent - now) + (double)1000000 / rounds_per_sec);
            }
        } else { //create new game
            everyone_is_ready = true;
            sem_wait(&clients_mutex);

            if (no_of_players < 2 || 25 < no_of_players) {
                sem_post(&clients_mutex);
                continue;
            }
            
            choose_players();

            if (!everyone_is_ready) {
                sem_post(&clients_mutex);
                continue;
            }
            no_of_players_alive = no_of_players;

            sem_wait(&events_mutex);
            events.clear();
            no_of_events = 0;
            sem_post(&events_mutex);
            
            sem_post(&clients_mutex);

            game_started = true;

            create_new_game();

            start_of_game = get_time_now();
        }
    }
}