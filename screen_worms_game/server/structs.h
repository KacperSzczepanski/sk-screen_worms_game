#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <algorithm>

#ifndef _STRUCTS_
#define _STRUCTS_

struct player_data_t {
    std::string name;
    double x;
    double y;
    bool is_eliminated;
    uint32_t direction;
    sockaddr_in6 addr;
};

struct client_data_t {
    uint64_t session_id;
    uint8_t direction;
    uint32_t next_expected_event_no;
    std::string player_name;
    uint64_t time_of_last_message;
    bool is_ready;
};

#endif