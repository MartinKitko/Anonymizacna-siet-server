#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include "downloader.h"
#include <pthread.h>
#include <stdbool.h>
#include <netinet/in.h>

#ifdef    __cplusplus
extern "C" {
#endif

#define USER_LENGTH 10
#define BUFFER_LENGTH 300
#define NUM_NODES 20

extern char *endMsg;
extern bool keepRunning;
extern int serverSocket;

typedef struct data {
    char userName[USER_LENGTH + 1];
    pthread_mutex_t mutex;
    int socket;
    int stop;
} DATA;

typedef struct node {
    int id;
    int socketIn;
    int socketOut;
    struct sockaddr_in address;
} NODE;

extern NODE nodes[NUM_NODES];

void *receiveAndForward(void *arg);
void *processMessage(void *arg);
void *stop(void *);

void printError(char *str);

#ifdef    __cplusplus
}
#endif

#endif    /* DEFINITIONS_H */

