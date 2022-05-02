# Computer networks - Screen game worms client + server (May 2021)
Server and client to game worms (snake multiplayer).

To run server: ./screen-worms-server [-p n] [-s n] [-t n] [-v n] [-w n] [-h n]

p port (2021)<br />
s seed (time(NULL))<br />
t turning speed (6)<br />
v game speed (50)<br />
w width of game (640)<br />
h hiehgt of game (480)<br />

To run client: ./screen-worms-client game_server [-n player_name] [-p n] [-i gui_server] [-r n]

game_server - address or name of server (IPv4 or IPv6)<br />
n playername (no name, but then its treated as spectator)<br />
p port (2021)<br />
i gui server (localhost)<br />
r port of gui (20210)<br />
