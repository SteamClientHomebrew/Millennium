#include <sys/socket.h>
#include <sys/un.h>
#include "sock_serv.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define SOCKET_PATH "/tmp/millennium_socket"

int send_message(enum Millennium id, char* data) {
    int sockfd;
    struct sockaddr_un addr;
    char buffer[1024];

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("connect");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    int needed_size = snprintf(NULL, 0, "%d|!|%s", (int)id, data) + 1;
    char *result = (char *)malloc(needed_size);

    if (!result) {
        perror("malloc failed");
        return 1;
    }

    snprintf(result, needed_size, "%d|!|%s", (int)id, data);

    send(sockfd, result, sizeof(result), 0);
    free(result);

    int bytesReceived = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        printf("%s\n", buffer);
    }

    close(sockfd);
    return 0;
}