#include "server_definitions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>

char *endMsg = ":end";

void *receiveAndForward(void *arg) {
    NODE *node = arg;
    int BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];
    printf("Noda %d zacina.\n", node->id);

    // prijatie spojenia od inej nody alebo klienta
    struct sockaddr_in connectingNodeAddress;
    socklen_t connNodeAddrLength = sizeof(connectingNodeAddress);
    int newSocket = accept(node->socketIn, (struct sockaddr *) &connectingNodeAddress, &connNodeAddrLength);
    if (newSocket < 0) {
        char msg[32];
        sprintf(msg, "Chyba - node %d accept.", node->id);
        printError(msg);
    }

    printf("Noda %d prijala spojenie\n", node->id);

    fd_set readfds;
    struct timeval timeout;
    int recvFrom, sendTo, received;

    while (keepRunning) {
        FD_ZERO(&readfds);
        FD_SET(newSocket, &readfds);
        FD_SET(node->socketOut, &readfds);

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        // cakanie na spravu od jedneho zo socketov
        int result = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout);
        if (result < 0) {
            printError("Chyba - node select.");
            break;
        } else if (result == 0) { // timeout
            continue;
        }

        if (FD_ISSET(newSocket, &readfds)) {
            recvFrom = newSocket;
            sendTo = node->socketOut;
        }

        if (FD_ISSET(node->socketOut, &readfds)) {
            recvFrom = node->socketOut;
            sendTo = newSocket;
        }

        received = recv(recvFrom, buffer, BUFFER_SIZE, 0);

        if (received < 0) {
            char msg[32];
            sprintf(msg, "Chyba - node %d recv.", node->id);
            printError(msg);
        } else if (received == 0) {
            break;
        }

        printf("Noda %d prijala spravu a posiela ju dalej.\n", node->id);

        // preposlanie spravy na socket, ktorym nebola prijata sprava
        if (send(sendTo, buffer, received, 0) < 0) {
            printError("Chyba - node send.");
        }
    }

    printf("Noda %d konci.\n", node->id);
    close(newSocket);
    return NULL;
}

void *processMessage(void *arg) {
    int *socket = arg;
    int received, result;

    int flags = fcntl(*socket, F_GETFL, 0);
    fcntl(*socket, F_SETFL, flags | O_NONBLOCK);

    while (keepRunning) {
        char buffer[BUFFER_LENGTH + 1];
        received = recv(*socket, buffer, sizeof(buffer), 0);
        if (received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                printError("Chyba - exit node recv");
            }
        } else if (received == 0) {
            printf("Spojenie ukoncene.\n");
            break;
        }
        buffer[received] = '\0';

        char *url = "";
        char username[BUFFER_LENGTH + 1] = "";
        char *colon = strstr(buffer, ": ");
        if (colon != NULL) {
            strncpy(username, buffer, colon - buffer);
            url = colon + 2;
        }

        if (strcmp(url, ":end") == 0) {
            printf("Pouzivatel ukoncil komunikaciu\n");
            if (send(*socket, buffer, BUFFER_LENGTH, 0) < 0) {
                printError("Chyba - exit node send");
            }
            break;
        }

        struct Downloader downloader;
        downloader_init(&downloader, url);
        result = downloader_download(&downloader);
        if (result == CURLE_OK) {
            printf("User: %s\n%s\n", username, downloader.content);
            int contentLength = strlen(downloader.content);
            int curPos = 0;
            while (keepRunning && curPos < contentLength) {
                int chunkLength = contentLength - curPos;
                if (chunkLength > BUFFER_LENGTH) {
                    chunkLength = BUFFER_LENGTH;
                }
                strncpy(buffer, downloader.content + curPos, chunkLength);
                if (send(*socket, buffer, chunkLength, 0) < 0) {
                    printError("Chyba - exit node send");
                }
                curPos += chunkLength;
            }
        } else {
            strncpy(buffer, "Stiahnutie neuspesne, skontrolujte URL.", BUFFER_LENGTH);
            if (send(*socket, buffer, BUFFER_LENGTH, 0) < 0) {
                printError("Chyba - exit node send");
            }
        }
        downloader_free(&downloader);
    }

    return NULL;
}

void *stop(void *unused) {
    char text[BUFFER_LENGTH];
    while (keepRunning && fgets(text, BUFFER_LENGTH, stdin) > 0) {
        char *pos = strchr(text, '\n');
        if (pos != NULL) {
            *pos = '\0';
        }

        if (strstr(text, endMsg) == text && strlen(text) == strlen(endMsg)) {
            keepRunning = false;
            printf("Koniec programu.\n");
        }
    }

    return NULL;
}

void printError(char *str) {
    if (errno != 0) {
        perror(str);
    } else {
        fprintf(stderr, "%s\n", str);
    }
    exit(EXIT_FAILURE);
}
