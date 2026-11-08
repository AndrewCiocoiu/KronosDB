#include "server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

void Server::init(){
    //This creates an IPv4 TCP socket and will return us its fileddescriptor
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    //This prevents address already in use
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    //This binds the socket to 0.0.0.0 (all ntwrok interfaces) on port 8081
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8081); //Hton will convert to big-endian
    bind(server_fd, (struct sockaddr*)&address, sizeof(address));

    //Marks the socket as passive aka ready to accept connections
    listen(server_fd, SOMAXCONN);
}