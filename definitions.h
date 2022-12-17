#ifndef DEFINITIONS_H
#define	DEFINITIONS_H

#include <pthread.h>
#include <stdbool.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define USER_LENGTH 10
#define BUFFER_LENGTH 300
extern char *endMsg;
extern bool keepRunning;

typedef struct data {
    char userName[USER_LENGTH + 1];
    pthread_mutex_t mutex;
    int socket;
    int stop;
} DATA;

void data_init(DATA *data, const char* userName, const int socket);
void data_destroy(DATA *data);
void data_stop(DATA *data);
int data_isStopped(DATA *data);
void *data_readData(void *data);
void *data_writeData(void *data);

void printError(char *str);

#ifdef	__cplusplus
}
#endif

#endif	/* DEFINITIONS_H */

