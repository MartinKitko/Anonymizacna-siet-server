#include "definitions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>

char *endMsg = ":end";

void data_init(DATA *data, const char *userName, const int socket) {
    data->socket = socket;
    data->stop = 0;
    data->userName[USER_LENGTH] = '\0';
    strncpy(data->userName, userName, USER_LENGTH);
    pthread_mutex_init(&data->mutex, NULL);
}

void data_destroy(DATA *data) {
    pthread_mutex_destroy(&data->mutex);
}

void data_stop(DATA *data) {
    pthread_mutex_lock(&data->mutex);
    data->stop = 1;
    pthread_mutex_unlock(&data->mutex);
}

int data_isStopped(DATA *data) {
    int stop;
    pthread_mutex_lock(&data->mutex);
    stop = data->stop;
    pthread_mutex_unlock(&data->mutex);
    return stop;
}

void *data_readData(void *data) {
    DATA *pdata = (DATA *) data;
    char buffer[BUFFER_LENGTH + 1];
    buffer[BUFFER_LENGTH] = '\0';
    struct timeval timeout;
    fd_set readfds;
    while (keepRunning && !data_isStopped(pdata)) {
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        FD_ZERO(&readfds);
        FD_SET(pdata->socket, &readfds);

        int result = select(pdata->socket + 1, &readfds, NULL, NULL, &timeout);
        if (result > 0) {
            bzero(buffer, BUFFER_LENGTH);
            if (read(pdata->socket, buffer, BUFFER_LENGTH) > 0) {
                char *posSemi = strchr(buffer, ':');
                char *pos = strstr(posSemi + 1, endMsg);
                if (pos != NULL && pos - posSemi == 2 && *(pos + strlen(endMsg)) == '\0') {
                    *(pos - 2) = '\0';
                    printf("Pouzivatel %s ukoncil komunikaciu.\n", buffer);
                    data_stop(pdata);
                } else {
                    printf("%s\n", buffer);
                }
            } else {
                data_stop(pdata);
            }
        } else if (result != 0) {
            printError("Chyba - readData select.");
        }
    }

    return NULL;
}

void *processMessage(void *arg) {
    int *socket = arg;
    fd_set readfds;
    struct timeval timeout;
    int received;

    while (keepRunning) {
        FD_ZERO(&readfds);
        FD_SET(*socket, &readfds);

        timeout.tv_sec = 1;
        int result = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout);
        if (result < 0) {
            printError("Chyba - exit node select");
            break;
        } else if (result == 0) { // timeout
            continue;
        }

        char buffer[BUFFER_LENGTH + 1];
        received = recv(*socket, buffer, sizeof(buffer), 0);
        if (received < 0) {
            printError("Chyba - exit node recv.");
        } else if (received == 0) {
            printf("Spojenie ukoncene.\n");
            return NULL;
        }
        buffer[received] = '\0';

        char username[BUFFER_LENGTH + 1];
        char *colon = strstr(buffer, ": ");
        if (colon != NULL) {
            strncpy(username, buffer, colon - buffer);
            username[colon - buffer] = '\0';
        }
        char *url = colon + 2;

        struct Downloader downloader;
        downloader_init(&downloader, url);
        result = downloader_download(&downloader);
        if (result == CURLE_OK) {
            printf("User: %s\n%s\n", username, downloader.content);
            strncpy(buffer, downloader.content, BUFFER_LENGTH);
        } else {
            strncpy(buffer, "Stiahnutie neuspesne, skontrolujte URL.", BUFFER_LENGTH);
        }
        downloader_free(&downloader);

        if (send(*socket, buffer, BUFFER_LENGTH, 0) < 0) {
            printError("Chyba - exit node send");
        }
    }

    return NULL;
}

void *stop(void *unused) {
    char *text = malloc(BUFFER_LENGTH);
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

    free(text);
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
