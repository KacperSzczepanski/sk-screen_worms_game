CLIENTSRC = client/screen-worms-client.cpp client/from_gui.cpp client/from_server_to_gui.cpp client/to_server.cpp
SERVERSRC = server/screen-worms-server.cpp server/events.cpp server/clients_management.cpp server/game_management.cpp 

CXX = g++ 

all: screen-worms-server screen-worms-client

screen-worms-client: 
	$(CXX) $(CLIENTSRC) -std=c++17 -Wall -Wextra -O2 -lpthread -o screen-worms-client
	
screen-worms-server: 
	$(CXX) $(SERVERSRC) -std=c++17 -Wall -Wextra -O2 -lpthread -o screen-worms-server

.PHONY: clean
clean:
	rm -rf *.o screen-worms-client screen-worms-server
