#include "from_server_to_gui.h"
#include "crc32_table.h"

#include <string.h>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <semaphore.h>
#include <iostream>

extern int sock_udp, sock_tcp;
extern uint32_t next_expected_event_no;
extern sem_t next_expected_event_mutex;

char buffer[MAX_DATAGRAM_SIZE];
int ptr;

bool never_played = true;
uint32_t cur_game_id;
uint32_t x, y, game_x, game_y, crc;

uint32_t event_len, event_no;
uint8_t event_type, player_no;

std::vector <std::string> players_names, names;

uint32_t crc32(char buf[], size_t start, size_t size)
{
    uint32_t crc;
    crc = ~0U;
    
    while (size--) {
        crc = crc32_tab[(crc ^ buf[start]) & 0xFF] ^ (crc >> 8);
		++start;
    }
    
    return crc ^ ~0U;
}

uint32_t read_bytes(int bytes) {
    uint32_t result = 0;

    for (int i = 0; i < bytes; ++i) {
        result <<= 8;
        result += (uint8_t)buffer[ptr++];
    }

    return result;
}

void read_players_names() {

    int number_of_names = 0;
    int length_of_name = 0;

    for (int i = 0; i < (int)event_len - 13; ++i) {
        
        if (buffer[ptr] == 0) {
            if (length_of_name == 0) {
                std::cerr << "Invalid NULL.\n";
                exit(EXIT_FAILURE);
            }

            names.push_back(std::string(buffer + ptr - length_of_name, buffer + ptr));
            ++number_of_names;
            length_of_name = 0;

            if (number_of_names > 25) {
                std::cerr << "Too many players.\n";
                exit(EXIT_FAILURE);
            }
        } else if (33 <= buffer[ptr] && buffer[ptr] <= 126) {
            ++length_of_name;

            if (length_of_name > 20) {
                std::cerr << "Name too long.\n";
                exit(EXIT_FAILURE);
            }
        } else {
            std::cerr << "Invalid character in name.\n";
            exit(EXIT_FAILURE);
        }
        
        ++ptr;
    }
}

void send_to_gui(std::string message) {
    int len = write(sock_tcp, message.c_str(), message.length());
    if (len != (int)message.length()) {
        std::cerr << "Error sending message to gui.\n";
        exit(EXIT_FAILURE);
    }
}

void *from_server_to_gui(void*) {
    int len;

    while ((len = read(sock_udp, buffer, 550)) >= 0) {
        ptr = 0;

        if (len < 4 || MAX_DATAGRAM_SIZE < len) {
            std::cerr << "Wrong received datagram's size.\n";
            exit(EXIT_FAILURE);
        }

        uint32_t game_id = read_bytes(4);

        for (;;) {
            if (ptr == len) {
                break;
            }

            bool ignore_event = false;

            std::string message_to_gui;

            if (ptr + 4 > len) {
                std::cerr << "Invalid message (event_len).\n";
                exit(EXIT_FAILURE);
            }
            event_len = read_bytes(4);

            if (ptr + 4 > len) {
                std::cerr << "Invalid message (event_no).\n";
                exit(EXIT_FAILURE);
            }
            event_no = read_bytes(4);

            if (ptr + 1 > len) {
                std::cerr << "Invalid message (event_type).\n";
                exit(EXIT_FAILURE);
            }
            event_type = read_bytes(1);

            sem_wait(&next_expected_event_mutex);
            if (next_expected_event_no != event_no) {
                if (event_type == 0 && event_no == 0 && (game_id != cur_game_id || never_played)) {
                    ignore_event = false;
                } else {
                    ignore_event = true;
                }
            }
            sem_post(&next_expected_event_mutex);

            if (event_type == 0) {
                if (event_no != 0) {
                    std::cerr << "Game should start with event no 0.\n";
                    exit(EXIT_FAILURE);
                }

                if ((game_id != cur_game_id || never_played) && !ignore_event) {
                    never_played = false;
                    cur_game_id = game_id;

                    message_to_gui = "NEW_GAME ";

                    sem_wait(&next_expected_event_mutex);
                    next_expected_event_no = 1;
                    sem_post(&next_expected_event_mutex);

                    if (ptr + 4 > len) {
                        std::cerr << "Invalid message (x).\n";
                        exit(EXIT_FAILURE);
                    }
                    x = read_bytes(4);
                    game_x = x;

                    message_to_gui += std::to_string(x);
                    message_to_gui += " ";

                    if (x < MIN_VAL || MAX_VAL < x) {
                        std::cerr << "Invalid size of map (x).\n";
                        exit(EXIT_FAILURE);
                    }

                    if (ptr + 4 > len) {
                        std::cerr << "Invalid message (y).\n";
                        exit(EXIT_FAILURE);
                    }
                    y = read_bytes(4);
                    game_y = y;

                    message_to_gui += std::to_string(y);

                    if (y < MIN_VAL || MAX_VAL < y) {
                        std::cerr << "Invalid size of map (y).\n";
                        exit(EXIT_FAILURE);
                    }

                    names.clear();
                    players_names.clear();
                    read_players_names();
                    std::sort(names.begin(), names.end());
                    players_names = names;

                    for (const std::string name : names) {
                        message_to_gui += " ";
                        message_to_gui += name;
                    }

                } else {
                    if (ptr + ((int)event_len - 5) > len) {
                        std::cerr << "Invalid message (game_id new_game event).\n";
                        exit(EXIT_FAILURE);
                    }
                    ptr += (event_len - 5);
                }
            } else if (event_type == 1) {
                if (game_id == cur_game_id && !ignore_event) {
                    message_to_gui = "PIXEL ";

                    sem_wait(&next_expected_event_mutex);
                    next_expected_event_no = event_no + 1;
                    sem_post(&next_expected_event_mutex);

                    if (ptr + 1 > len) {
                        std::cerr << "Invalid message (pixel player_number).\n";
                        exit(EXIT_FAILURE);
                    }
                    player_no = read_bytes(1);
                    if (player_no >= players_names.size()) {
                        std::cerr << "Player does not exist (pixel).\n";
                        exit(EXIT_FAILURE);
                    }

                    if (ptr + 4 > len) {
                        std::cerr << "Invalid message (pixel x).\n";
                        exit(EXIT_FAILURE);
                    }
                    x = read_bytes(4);
                    message_to_gui += std::to_string(x);
                    message_to_gui += " ";
                    if (game_x < x) {
                        std::cerr << "Invalid coordinate (pixel x).\n";
                        exit(EXIT_FAILURE);
                    }

                    if (ptr + 4 > len) {
                        std::cerr << "Invalid message (pixel y).\n";
                        exit(EXIT_FAILURE);
                    }
                    y = read_bytes(4);
                    message_to_gui += std::to_string(y);
                    message_to_gui += " ";
                    if (game_y < y) {
                        std::cerr << "Invalid coordinate (pixel y).\n";
                        exit(EXIT_FAILURE);
                    }

                    message_to_gui += players_names[player_no];
                } else {
                    if (ptr + ((int)event_len - 5) > len) {
                        std::cerr << "Invalid message (game_id pixel event).\n";
                        exit(EXIT_FAILURE);
                    }
                    ptr += (event_len - 5);
                }
            } else if (event_type == 2) {
                if (game_id == cur_game_id && !ignore_event) {
                    sem_wait(&next_expected_event_mutex);
                    next_expected_event_no = event_no + 1;
                    sem_post(&next_expected_event_mutex);

                    if (ptr + 1 > len) {
                        std::cerr << "Invalid message (eliminated player_no).\n";
                        exit(EXIT_FAILURE);
                    }
                    player_no = read_bytes(1);
                    if (player_no >= players_names.size()) {
                        std::cerr << "Player does not exist (pixel).\n";
                        exit(EXIT_FAILURE);
                    }

                    message_to_gui = "PLAYER_ELIMINATED ";
                    message_to_gui += std::to_string(player_no);
                } else {
                    if (ptr + ((int)event_len - 5) > len) {
                        std::cerr << "Invalid message (game_id eliminated event).\n";
                        exit(EXIT_FAILURE);
                    }
                    ptr += (event_len - 5);
                }
            } else if (event_type == 3) {
                if (!ignore_event) {    
                    sem_wait(&next_expected_event_mutex);
                    next_expected_event_no = event_no + 1;
                    sem_post(&next_expected_event_mutex);
                }
            } else {
                if (ptr + ((int)event_len - 5) > len) {
                    std::cerr << "Invalid message (game_id unknown event).\n";
                    exit(EXIT_FAILURE);
                }
            }

            if (ptr + 4 > len) {
                std::cerr << "Invalid message (crc).\n";
                exit(EXIT_FAILURE);
            }

            crc = read_bytes(4);
            uint32_t correct_crc = crc32(buffer, ptr - (event_len + 8), event_len + 4);

            if (game_id != cur_game_id || ignore_event) {
                continue;
            }

            if (crc != correct_crc) {
                break;
            }

            if (event_type < 3) {
                message_to_gui += char(10);
                send_to_gui(message_to_gui);
            }
        }
    }
    return 0;
}