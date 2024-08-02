#include <cstring>
#include <print>
#include <sys/_types/_socklen_t.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 12345

using namespace std;

int start_host() {
    
    int host_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (host_fd < 0) {
        println("Failed to create socket");
        return -1;
    }

    int opt_val = 1;
    setsockopt(host_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));

    struct sockaddr_in host;
    host.sin_family = AF_INET;
    host.sin_port = htons(PORT);
    host.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(host_fd, (struct sockaddr*)&host, sizeof(host)) < 0) {
        println("Failed to bind");
        return -1;
    }

    if (listen(host_fd, 128) < 0) {
        println("Failed to listen to socket");
        return -1;
    }

    println("Listening on port {}", PORT);

    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);
    int client_fd = accept(host_fd, (struct sockaddr*)&client, &client_len);

    if (client_fd < 0) {
        println("Failed to accept incoming connection");
    }

    if (send(client_fd, "Hello friend", 13, 0) < 0) {
        println("Failed to send greetings");
    }

    println("Successfully sent greeting");

    return 0;
}

int start_client() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        println("Failed to create socket");
        return -1;
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(server_fd, (struct sockaddr*)&server, sizeof(server)) < 0) {
        println("Failed to connect to server");
        return -1;
    }

    char buff[1024];
    int msg_len = recv(server_fd, &buff, sizeof(buff), 0);
    if (msg_len < 0) {
        println("Failed to read from server");
        return -1;
    }

    char* msg = (char*)malloc(msg_len);
    memcpy(msg, buff, msg_len);

    println("Received from server: {}", msg);

    free(msg);

    return 0;
}
