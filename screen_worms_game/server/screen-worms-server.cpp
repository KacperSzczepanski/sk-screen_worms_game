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
#include <map>

#include "clients_management.h"
#include "game_management.h"
#include "structs.h"

using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::system_clock;

uint32_t port = 2021;
uint32_t seed = time(NULL);
uint32_t turning_speed = 6;
uint32_t rounds_per_sec = 50;
uint32_t width = 640;
uint32_t height = 480;
int sock;

pthread_t game_management_thread;
pthread_t clients_management_thread;
pthread_attr_t thread_attr;

uint8_t no_of_players;

std::map <sockaddr_in6, client_data_t> clients;
sem_t clients_mutex, events_mutex;

bool operator<(player_data_t player1, player_data_t player2) {
    int len = std::min(player1.name.length(), player2.name.length());

    for (int i = 0; i < len; ++i) {
        if (player1.name.at(i) != player2.name.at(i)) {
            return player1.name.at(i) < player2.name.at(i);
        }
    }

    return player1.name.length() < player2.name.length();
}
bool operator<(sockaddr_in6 addr1, sockaddr_in6 addr2) {
    for (int i = 0; i < 4; i++)
    {
        if (addr1.sin6_addr.__in6_u.__u6_addr32[i] != addr2.sin6_addr.__in6_u.__u6_addr32[i])
        {
            return addr1.sin6_addr.__in6_u.__u6_addr32[i] < addr2.sin6_addr.__in6_u.__u6_addr32[i];
        }
    }

    return addr1.sin6_port < addr2.sin6_port;
}
bool operator==(sockaddr_in6 addr1, sockaddr_in6 addr2) {
    return (!(addr1 < addr2) && !(addr2 < addr1));
}

uint64_t get_time_now() {
    auto millisec_since_epoch = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    uint64_t now = static_cast<long long int>(millisec_since_epoch);

    return now;
}

int setup_connection(uint32_t port) {
    struct sockaddr_in6 addr_hints {};

    int sock;
    if ((sock = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
        std::cerr << "Opening socket failed.\n";
        exit(EXIT_FAILURE);
    }

    addr_hints.sin6_family = AF_INET6;
    addr_hints.sin6_addr = in6addr_any;
    addr_hints.sin6_port = htons(port);

    if (bind(sock, (struct sockaddr *)&addr_hints, (socklen_t)sizeof(addr_hints)) < 0) {
        std::cerr << "Binding failed.\n";
        exit(EXIT_FAILURE);
    }
        
    return sock;
}

void check_if_number(const std::string &number, std::string message) {
    if (number.length() == 0 && number.at(0) != '0') {
        std::cerr << message << " is not a number.\n";
        exit(EXIT_FAILURE); 
    }

    for (const char &c : number) {
        if (int(c) < 48 || 57 < int(c)) {
            std::cerr << message << " is not a number.\n";
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char* argv[]) {

    int opt;
    while ((opt = getopt(argc, argv, "p:s:t:v:w:h:")) != -1) {
        switch (opt) {
            case 'p':
                check_if_number(optarg, "Port");
                port = atoi(optarg);
                break;
            case 's':
                check_if_number(optarg, "Seed");
                seed = atoi(optarg);
                break;
            case 't':
                check_if_number(optarg, "Turning speed");
                turning_speed = atoi(optarg);
                break;
            case 'v':
                check_if_number(optarg, "Rounds per sec");
                rounds_per_sec = atoi(optarg);
                break;
            case 'w':
                check_if_number(optarg, "Width");
                width = atoi(optarg);
                break;
            case 'h':
                check_if_number(optarg, "Height");
                height = atoi(optarg);
                break;
            default: /* '?' */
                puts ("XD");
                std::cerr << "Usage: " << argv[0] << " game_server [-n player_name] [-p n] [-i gui_server] [-r n]\n";
                exit(EXIT_FAILURE);
        }
    }

    std::cerr << "Game port: " << port << "\n";
    std::cerr << "Seed: " << seed << "\n";
    std::cerr << "Turning speed: " << turning_speed << "\n";
    std::cerr << "Rounds per sec: " << rounds_per_sec << "\n";
    std::cerr << "Width: " << width << "\n";
    std::cerr << "Height: " << height << "\n";
    sem_init(&clients_mutex, 0, 1);
    sem_init(&events_mutex, 0, 1);

    if (pthread_attr_init(&thread_attr) != 0)
    {
        std::cerr << "pthread_attr_init\n";
        exit(EXIT_FAILURE);
    }

    if (pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE) != 0)
    {
        std::cerr << "Could not set detach state.\n";
        exit(EXIT_FAILURE);
    }

    sock = setup_connection(port);

    if (pthread_create(&clients_management_thread, &thread_attr, clients_management, NULL) != 0) {
        std::cerr << "Could not create thread (clients).\n";
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&game_management_thread, &thread_attr, game_management, NULL) != 0) {
        std::cerr << "Could not create thread (game).\n";
        exit(EXIT_FAILURE);
    }

    if (pthread_join(clients_management_thread, NULL) != 0) {
        std::cerr << "Could not join thread (clients).\n";
        exit(EXIT_FAILURE);
    }

    if (pthread_join(game_management_thread, NULL) != 0) {
        std::cerr << "Could not join thread (game).\n";
        exit(EXIT_FAILURE);
    }

    return 0;
}