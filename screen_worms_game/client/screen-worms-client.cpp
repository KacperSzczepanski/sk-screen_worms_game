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

#include "from_gui.h"
#include "from_server_to_gui.h"
#include "to_server.h"
#include "crc32_table.h"

using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::system_clock;

std::string player_name = "";
std::string server_address;
std::string server_port = "2021";
std::string gui_address = "localhost";
std::string gui_port = "20210";

uint64_t session_id;
uint8_t direction;
uint32_t next_expected_event_no;
sem_t direction_mutex, next_expected_event_mutex;
int sock_udp, sock_tcp;

pthread_t from_gui_thread;
pthread_t from_server_to_gui_thread;
pthread_t to_server_thread;
pthread_attr_t thread_attr;

int connect_to_server(const std::string &address, const std::string &port) /* UDP */ {
    
    struct addrinfo addr_hints;
    struct addrinfo *addr_result;

    memset(&addr_hints, 0, sizeof(struct addrinfo));

    addr_hints.ai_family = AF_UNSPEC;
    addr_hints.ai_socktype = SOCK_DGRAM;
    addr_hints.ai_protocol = IPPROTO_UDP;
    addr_hints.ai_flags = 0;
    addr_hints.ai_addrlen = 0;
    addr_hints.ai_addr = NULL;
    addr_hints.ai_canonname = NULL;
    addr_hints.ai_next = NULL;

    if (getaddrinfo(address.c_str(), port.c_str(), &addr_hints, &addr_result) != 0)
    {
        std::cerr << "getaddrinfo UDP\n";
        exit(EXIT_FAILURE);
    }

    int sock = socket(addr_result->ai_family, SOCK_DGRAM, addr_result->ai_protocol);
    if (sock < 0)
    {
        std::cerr << "socket UDP\n";
        exit(EXIT_FAILURE);
    }

    if (connect(sock, addr_result->ai_addr, addr_result->ai_addrlen) < 0)
    {
        std::cerr << "Could not connect to server.\n";
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(addr_result);

    return sock;
}
int connect_to_gui(std::string &address, std::string &port) /* TCP */ {
    
    struct addrinfo addr_hints;
    struct addrinfo *addr_result;

    memset(&addr_hints, 0, sizeof(struct addrinfo));
    addr_hints.ai_family = AF_UNSPEC;
    addr_hints.ai_socktype = SOCK_STREAM;
    addr_hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(address.c_str(), port.c_str(), &addr_hints, &addr_result) != 0) {
        std::cerr << "getaddrinfo TCP\n";
        exit(EXIT_FAILURE);
    }

    int sock = socket(addr_result->ai_family, addr_result->ai_socktype, addr_result->ai_protocol);
    if (sock < 0)
    {
        std::cerr << "socket TCP\n";
        exit(EXIT_FAILURE);
    }

    const int flag = 1;
    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void *) &flag, sizeof(int)) < 0)
    {
        std::cerr << "setsockopt TCP\n";
        exit(EXIT_FAILURE);
    }

    if (connect(sock, addr_result->ai_addr, addr_result->ai_addrlen) < 0)
    {
        std::cerr << "Could not connect to gui.\n";
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(addr_result);

    return sock;
}

void check_valid_name(const std::string &name) {
    if (name.length() < 1 || 20 < name.length()) {
        std::cerr << "Wrong name length.\n";
        exit(EXIT_FAILURE);
    }

    for (const char &c : name) {
        if (int(c) < 33 || 126 < int(c)) {
            std::cerr << "Wrong character in player's name.\n";
            exit(EXIT_FAILURE);
        }
    }
}

void check_is_number(const std::string &number) {
    if (number.length() == 0 && number.at(0) != '0') {
        std::cerr << "Port is not a number.\n";
        exit(EXIT_FAILURE); 
    }

    for (const char &c : number) {
        if (int(c) < 48 || 57 < int(c)) {
            std::cerr << "Port is not a number.\n";
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char* argv[]) {
    auto millisec_since_epoch = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

    session_id = static_cast<long long int>(millisec_since_epoch);

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " game_server [-n player_name] [-p n] [-i gui_server] [-r n]\n";
        exit(EXIT_FAILURE);
    }

    server_address = argv[1];

    int opt;
    while ((opt = getopt(argc, argv, "n:p:i:r:")) != -1) {
        switch (opt) {
            case 'n':
                check_valid_name(optarg);
                player_name = optarg;
                break;
            case 'p':
                check_is_number(optarg);
                server_port = optarg;
                break;
            case 'i':
                gui_address = optarg;
                break;
            case 'r':
                check_is_number(optarg);
                gui_port = optarg;
                break;
            default: /* '?' */
                std::cerr << "Usage: " << argv[0] << " game_server [-n player_name] [-p n] [-i gui_server] [-r n]\n";
                exit(EXIT_FAILURE);
        }
    }

    std::cerr << "Server address: " << server_address << "\n";
    std::cerr << "Server port: " << server_port << "\n";
    std::cerr << "Gui address: " << gui_address << "\n";
    std::cerr << "Gui port: " << gui_port << "\n";
    std::cerr << "Player's name: " << player_name << "\n";
    std::cerr << "Session id: " << session_id << "\n";

    sem_init(&direction_mutex, 0, 1);
    sem_init(&next_expected_event_mutex, 0, 1);

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

    sock_udp = connect_to_server(server_address, server_port);
    sock_tcp = connect_to_gui(gui_address, gui_port);

    if (pthread_create(&from_gui_thread, &thread_attr, from_gui, NULL) != 0) {
        std::cerr << "Could not create thread (from gui).\n";
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&from_server_to_gui_thread, &thread_attr, from_server_to_gui, NULL) != 0) {
        std::cerr << "Could not create thread (server-gui).\n";
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&to_server_thread, &thread_attr, to_server, NULL) != 0) {
        std::cerr << "Could not create thread (to server).\n";
        exit(EXIT_FAILURE);
    }
    
    if (pthread_join(from_gui_thread, NULL) != 0) {
        std::cerr << "Could not join thread (from gui).\n";
        exit(EXIT_FAILURE);
    }
    
    if (pthread_join(from_server_to_gui_thread, NULL) != 0) {
        std::cerr << "Could not join thread (server-gui).\n";
        exit(EXIT_FAILURE);
    }

    if (pthread_join(to_server_thread, NULL) != 0) {
        std::cerr << "Could not join thread (to server).\n";
        exit(EXIT_FAILURE);
    }

    return 0;
}