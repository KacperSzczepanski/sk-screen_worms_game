# Computer networks - Screen game worms client + server (May 2021)
Server and client to game worms (snake multiplayer).

To run server: ./screen-worms-server [-p n] [-s n] [-t n] [-v n] [-w n] [-h n]

-p port (2021)
-s seed (time(NULL))
-t turning speed (6)
-v game speed (50)
-w width of game (640)
-h hiehgt of game (480)

To run client: ./screen-worms-client game_server [-n player_name] [-p n] [-i gui_server] [-r n]

game_server - address or name of server (IPv4 or IPv6)
-n playername (no name, but then its treated as spectator)
-p port (2021)
-i gui server (localhost)
-r port of gui (20210)
