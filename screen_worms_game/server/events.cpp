#include <iostream>
#include <vector>
#include <string.h>

#include "events.h"

extern uint32_t no_of_events;

std::vector <uint8_t> new_game_event(uint32_t maxx, uint32_t maxy, std::vector <std::string> players) {
    std::vector <uint8_t> event;
    event.push_back((uint8_t)0);

    for (int i = 3; i >= 0; --i) {
        event.push_back((uint8_t)(maxx >> (i * 8)));
    }
    for (int i = 3; i >= 0; --i) {
        event.push_back((uint8_t)(maxy >> (i * 8)));
    }

    for (std::string player_name : players) {
        for (const char &c : player_name) {
            event.push_back((uint8_t)c);
        }

        event.push_back((uint8_t)0);
    }

    return event;
}

std::vector <uint8_t> pixel_event(uint8_t player_number, uint32_t x, uint32_t y) {
    std::vector <uint8_t> event;
    event.push_back((uint8_t)1);

    event.push_back((uint8_t)player_number);

    for (int i = 3; i >= 0; --i) {
        event.push_back((uint8_t)(x >> (i * 8)));
    }
    for (int i = 3; i >= 0; --i) {
        event.push_back((uint8_t)(y >> (i * 8)));
    }

    return event;
}

std::vector <uint8_t> player_eliminated_event(uint8_t player_number) {
    std::vector <uint8_t> event;
    event.push_back((uint8_t)2);

    event.push_back((uint8_t)player_number);

    return event;
}

std::vector <uint8_t> game_over_event() {
    std::vector <uint8_t> event;
    event.push_back((uint8_t)3);

    return event;
}