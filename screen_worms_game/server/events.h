#include <iostream>
#include <vector>
#include <string.h>

#ifndef _EVENTS_
#define _EVENTS_

std::vector <uint8_t> new_game_event(uint32_t maxx, uint32_t maxy, std::vector <std::string> players);
std::vector <uint8_t> pixel_event(uint8_t player_number, uint32_t x, uint32_t y);
std::vector <uint8_t> player_eliminated_event(uint8_t player_number);
std::vector <uint8_t> game_over_event();
 
#endif