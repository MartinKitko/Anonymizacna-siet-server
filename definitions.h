#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include <pthread.h>
#include <stdbool.h>
#include <netinet/in.h>

#ifdef    __cplusplus
extern "C" {
#endif

#define USER_LENGTH 10
#define BUFFER_LENGTH 300
#define MAX_CLIENTS 10
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

typedef struct client {
    pthread_t thread;
    int socket;
    DATA data;
} CLIENT;

typedef struct node {
    int id;
    int socketIn;
    int socketOut;
    struct sockaddr_in address;
} NODE;

extern NODE nodes[NUM_NODES];

void data_init(DATA *data, const char *userName, const int socket);
void data_destroy(DATA *data);
void data_stop(DATA *data);
int data_isStopped(DATA *data);
void *data_readData(void *data);
void *stop(void *);

void printError(char *str);

#ifdef    __cplusplus
}
#endif

#endif    /* DEFINITIONS_H */

