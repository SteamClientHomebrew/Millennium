#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <sock_serv.h>

#define SOCKET_PATH "/tmp/millennium_socket"
#define BUFFER_SIZE 1024

int send_message(Millennium id, char* data) {
    int sockfd;
    struct sockaddr_un addr;
    char buffer[BUFFER_SIZE];

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        std::cerr << "Failed to create a connection to Millennium. Logs are instance based so Millennium must be running to view them." << std::endl;
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    int needed_size = snprintf(NULL, 0, "%d|!|%s", (int)id, data) + 1;
    char *result = (char *)malloc(needed_size);

    if (!result) {
        perror("malloc failed");
        close(sockfd);
        return 1;
    }

    snprintf(result, needed_size, "%d|!|%s", (int)id, data);

    ssize_t bytes_sent = send(sockfd, result, needed_size - 1, 0); 
    if (bytes_sent == -1) {
        perror("send failed");
        free(result);
        close(sockfd);
        return 1;
    }

    free(result); 

    ssize_t bytes_received;
    while ((bytes_received = recv(sockfd, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';  
        printf("%s", buffer); 
        fflush(stdout);
    }

    if (bytes_received == -1) {
        perror("recv");
    }
    close(sockfd);
    return 0;
}
